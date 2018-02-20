#ifndef TASK_SPECIFICATION_CC
#define TASK_SPECIFICATION_CC

#include "TaskSpecification.h"

#include "common.h"
#include "common_protocol.h"

using namespace std;
namespace ray {

static const ObjectID task_compute_return_id(TaskID task_id, int64_t return_index) {
  /* Here, return_indices need to be >= 0, so we can use negative
   * indices for put. */
  RAY_DCHECK(return_index >= 0);
  /* TODO(rkn): This line requires object and task IDs to be the same size. */
  ObjectID return_id = task_id;
  int64_t *first_bytes = (int64_t *) &return_id;
  /* XOR the first bytes of the object ID with the return index. We add one so
   * the first return ID is not the same as the task ID. */
  *first_bytes = *first_bytes ^ (return_index + 1);
  return return_id;
}

TaskSpecification::TaskSpecification(const uint8_t *spec, size_t spec_size) {
  spec_.assign(spec, spec + spec_size);
}

TaskSpecification::TaskSpecification(const TaskSpecification &spec) {
  spec_.assign(spec.Data(), spec.Data() + spec.Size());
}

TaskSpecification::TaskSpecification(
    UniqueID driver_id,
    TaskID parent_task_id,
    int64_t parent_counter,
    //UniqueID actor_id,
    //UniqueID actor_handle_id,
    //int64_t actor_counter,
    FunctionID function_id,
    const std::vector<TaskArgument> &task_arguments,
    int64_t num_returns,
    const unordered_map<std::string, double> &required_resources) {
  flatbuffers::FlatBufferBuilder fbb;

  // Compute hashes.
  SHA256_CTX ctx;
  sha256_init(&ctx);
  sha256_update(&ctx, (BYTE *) &driver_id, sizeof(driver_id));
  sha256_update(&ctx, (BYTE *) &parent_task_id, sizeof(parent_task_id));
  sha256_update(&ctx, (BYTE *) &parent_counter, sizeof(parent_counter));
  //sha256_update(&ctx, (BYTE *) &actor_id, sizeof(actor_id));
  //sha256_update(&ctx, (BYTE *) &actor_counter, sizeof(actor_counter));
  //sha256_update(&ctx, (BYTE *) &is_actor_checkpoint_method,
  //              sizeof(is_actor_checkpoint_method));
  sha256_update(&ctx, (BYTE *) &function_id, sizeof(function_id));

  // Serialize and hash the arguments.
  std::vector<flatbuffers::Offset<Arg>> arguments;
  for (auto &argument : task_arguments) {
    arguments.push_back(argument.ToFlatbuffer());
    sha256_update(&ctx, (BYTE *) argument.HashData(), argument.HashDataLength());
  }

  // Compute the final task ID from the hash.
  BYTE buff[DIGEST_SIZE];
  sha256_final(&ctx, buff);
  TaskID task_id;
  CHECK(sizeof(task_id) <= DIGEST_SIZE);
  memcpy(&task_id, buff, sizeof(task_id));

  // Add return object IDs.
  std::vector<flatbuffers::Offset<flatbuffers::String>> returns;
  for (int64_t i = 0; i < num_returns; i++) {
    ObjectID return_id = task_compute_return_id(task_id, i);
    returns.push_back(to_flatbuf(fbb, return_id));
  }

  // Serialize the TaskSpecification.
  auto spec = CreateTaskInfo(
      fbb, to_flatbuf(fbb, driver_id), to_flatbuf(fbb, task_id),
      to_flatbuf(fbb, parent_task_id), parent_counter,
      to_flatbuf(fbb, WorkerID::nil()), to_flatbuf(fbb, ActorHandleID::nil()),
      0, false,
      to_flatbuf(fbb, function_id), fbb.CreateVector(arguments), fbb.CreateVector(returns),
      map_to_flatbuf(fbb, required_resources));
  fbb.Finish(spec);
  TaskSpecification(fbb.GetBufferPointer(), fbb.GetSize());
}

// TODO(atumanov): copy/paste most TaskSpec_* methods from task.h and make them
// methods of this class.
const uint8_t *TaskSpecification::Data() const {
  return spec_.data();
}

size_t TaskSpecification::Size() const {
  return spec_.size();
}

}

#endif
