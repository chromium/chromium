// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager.h"

#include <utility>

namespace base::sequence_manager {

namespace {

perfetto::protos::pbzero::SequenceManagerTask::Priority
DefaultTaskPriorityToProto(TaskQueue::QueuePriority priority) {
  DCHECK_EQ(priority, static_cast<TaskQueue::QueuePriority>(
                          TaskQueue::DefaultQueuePriority::kNormalPriority));
  return perfetto::protos::pbzero::SequenceManagerTask::Priority::
      NORMAL_PRIORITY;
}

void CheckPriorities(TaskQueue::QueuePriority priority_count,
                     TaskQueue::QueuePriority default_priority) {
  CHECK_LE(static_cast<size_t>(priority_count),
           SequenceManager::PrioritySettings::kMaxPriorities)
      << "The number of priorities cannot exceed kMaxPriorities.";
  CHECK_LT(static_cast<size_t>(default_priority), priority_count)
      << "The default priority must be within the priority range.";
}

}  // namespace

// static
SequenceManager::PrioritySettings
SequenceManager::PrioritySettings::CreateDefault() {
  PrioritySettings settings(
      TaskQueue::DefaultQueuePriority::kQueuePriorityCount,
      TaskQueue::DefaultQueuePriority::kNormalPriority);
  settings.SetProtoPriorityConverter(&DefaultTaskPriorityToProto);
  settings.SetThreadTypeMapping(&DefaultTaskPriorityToThreadType);
  return settings;
}

ThreadType SequenceManager::PrioritySettings::DefaultTaskPriorityToThreadType(
    TaskQueue::QueuePriority priority) {
  return ThreadType::kMaxValue;
}

SequenceManager::PrioritySettings::PrioritySettings(
    TaskQueue::QueuePriority priority_count,
    TaskQueue::QueuePriority default_priority)
    : priority_count_(priority_count), default_priority_(default_priority) {
  CheckPriorities(priority_count, default_priority);
}

perfetto::protos::pbzero::SequenceManagerTask::Priority
SequenceManager::PrioritySettings::TaskPriorityToProto(
    TaskQueue::QueuePriority priority) const {
  // `proto_priority_converter_` will be null in some unit tests, but those
  // tests should not be tracing.
  DCHECK(proto_priority_converter_)
      << "A tracing priority-to-proto-priority function was not provided";
  return proto_priority_converter_(priority);
}

ThreadType SequenceManager::PrioritySettings::TaskPriorityToThreadType(
    TaskQueue::QueuePriority priority) const {
  DCHECK(thread_type_mapping_);
  return thread_type_mapping_(priority);
}

SequenceManager::PrioritySettings::~PrioritySettings() = default;

SequenceManager::PrioritySettings::PrioritySettings(
    PrioritySettings&&) noexcept = default;

SequenceManager::PrioritySettings& SequenceManager::PrioritySettings::operator=(
    PrioritySettings&&) = default;

SequenceManager::Settings::Settings() = default;

SequenceManager::Settings::Settings(Settings&& move_from) noexcept = default;

SequenceManager::Settings::~Settings() = default;

SequenceManager::Settings::Builder::Builder() = default;

SequenceManager::Settings::Builder::~Builder() = default;

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetMessagePumpType(
    MessagePumpType message_loop_type_val) {
  settings_.message_loop_type = message_loop_type_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetShouldSampleCPUTime(bool enable) {
  settings_.sample_cpu_time = enable;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetTickClock(const TickClock* clock_val) {
  settings_.clock = clock_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetAddQueueTimeToTasks(
    bool add_queue_time_to_tasks_val) {
  settings_.add_queue_time_to_tasks = add_queue_time_to_tasks_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetCanRunTasksByBatches(
    bool can_run_tasks_by_batches_val) {
  settings_.can_run_tasks_by_batches = can_run_tasks_by_batches_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetPrioritySettings(
    SequenceManager::PrioritySettings settings) {
  settings_.priority_settings = std::move(settings);
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetIsMainThread(bool is_main_thread_val) {
  settings_.is_main_thread = is_main_thread_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetShouldReportLockMetrics(bool enable) {
  settings_.should_report_lock_metrics = enable;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetShouldBlockOnScopedFences(bool enable) {
  settings_.should_block_on_scoped_fences = enable;
  return *this;
}

#if DCHECK_IS_ON()

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetRandomTaskSelectionSeed(
    uint64_t random_task_selection_seed_val) {
  settings_.random_task_selection_seed = random_task_selection_seed_val;
  return *this;
}

#endif  // DCHECK_IS_ON()

SequenceManager::Settings SequenceManager::Settings::Builder::Build() {
  return std::move(settings_);
}

SequenceManagerSettings::SequenceManagerSettings(
    SequenceManager::Settings settings)
    : settings(std::move(settings)) {}

}  // namespace base::sequence_manager
