// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/sequence_manager.h"

#include <utility>

namespace base {
namespace sequence_manager {

namespace {

#if BUILDFLAG(ENABLE_BASE_TRACING)
perfetto::protos::pbzero::SequenceManagerTask::Priority
DefaultTaskPriorityToProto(TaskQueue::QueuePriority priority) {
  DCHECK_EQ(priority, static_cast<TaskQueue::QueuePriority>(
                          TaskQueue::DefaultQueuePriority::kNormalPriority));
  return perfetto::protos::pbzero::SequenceManagerTask::Priority::
      NORMAL_PRIORITY;
}
#endif

void CheckPriorities(TaskQueue::QueuePriority priority_count,
                     TaskQueue::QueuePriority default_priority) {
  CHECK_LE(static_cast<size_t>(priority_count),
           SequenceManager::PrioritySettings::kMaxPriorities)
      << "The number of priorities cannot exceed kMaxPriorities.";
  CHECK_LT(static_cast<size_t>(default_priority), priority_count)
      << "The default priority must be within the priority range.";
}

}  // namespace

SequenceManager::MetricRecordingSettings::MetricRecordingSettings(
    double task_thread_time_sampling_rate)
    : task_sampling_rate_for_recording_cpu_time(
          base::ThreadTicks::IsSupported() ? task_thread_time_sampling_rate
                                           : 0) {}

// static
SequenceManager::PrioritySettings
SequenceManager::PrioritySettings::CreateDefault() {
  PrioritySettings settings(
      TaskQueue::DefaultQueuePriority::kQueuePriorityCount,
      TaskQueue::DefaultQueuePriority::kNormalPriority);
#if BUILDFLAG(ENABLE_BASE_TRACING)
  settings.SetProtoPriorityConverter(&DefaultTaskPriorityToProto);
#endif
  return settings;
}

SequenceManager::PrioritySettings::PrioritySettings(
    TaskQueue::QueuePriority priority_count,
    TaskQueue::QueuePriority default_priority)
#if DCHECK_IS_ON()
    : PrioritySettings(priority_count,
                       default_priority,
                       std::vector<TimeDelta>(priority_count),
                       std::vector<TimeDelta>(priority_count)){}
#else
    : priority_count_(priority_count), default_priority_(default_priority) {
  CheckPriorities(priority_count, default_priority);
}
#endif

#if DCHECK_IS_ON()
      SequenceManager::PrioritySettings::PrioritySettings(
          TaskQueue::QueuePriority priority_count,
          TaskQueue::QueuePriority default_priority,
          std::vector<TimeDelta> per_priority_cross_thread_task_delay,
          std::vector<TimeDelta> per_priority_same_thread_task_delay)
    : priority_count_(priority_count),
      default_priority_(default_priority),
      per_priority_cross_thread_task_delay_(
          std::move(per_priority_cross_thread_task_delay)),
      per_priority_same_thread_task_delay_(
          std::move(per_priority_same_thread_task_delay)) {
  CheckPriorities(priority_count, default_priority);
  DCHECK_EQ(priority_count, per_priority_cross_thread_task_delay_.size());
  DCHECK_EQ(priority_count, per_priority_same_thread_task_delay_.size());
}
#endif

#if BUILDFLAG(ENABLE_BASE_TRACING)
perfetto::protos::pbzero::SequenceManagerTask::Priority
SequenceManager::PrioritySettings::TaskPriorityToProto(
    TaskQueue::QueuePriority priority) const {
  // `proto_priority_converter_` will be null in some unit tests, but those
  // tests should not be tracing.
  DCHECK(proto_priority_converter_)
      << "A tracing priority-to-proto-priority function was not provided";
  return proto_priority_converter_(priority);
}
#endif

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
SequenceManager::Settings::Builder::SetRandomisedSamplingEnabled(
    bool randomised_sampling_enabled_val) {
  settings_.randomised_sampling_enabled = randomised_sampling_enabled_val;
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

#if DCHECK_IS_ON()

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetRandomTaskSelectionSeed(
    uint64_t random_task_selection_seed_val) {
  settings_.random_task_selection_seed = random_task_selection_seed_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetTaskLogging(
    TaskLogging task_execution_logging_val) {
  settings_.task_execution_logging = task_execution_logging_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetLogPostTask(bool log_post_task_val) {
  settings_.log_post_task = log_post_task_val;
  return *this;
}

SequenceManager::Settings::Builder&
SequenceManager::Settings::Builder::SetLogTaskDelayExpiry(
    bool log_task_delay_expiry_val) {
  settings_.log_task_delay_expiry = log_task_delay_expiry_val;
  return *this;
}

#endif  // DCHECK_IS_ON()

SequenceManager::Settings SequenceManager::Settings::Builder::Build() {
  return std::move(settings_);
}

}  // namespace sequence_manager
}  // namespace base
