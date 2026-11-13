// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/responsiveness/watcher.h"

#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/pending_task.h"
#include "base/task/current_thread.h"
#include "base/task/responsiveness/calculator.h"
#include "base/task/responsiveness/message_loop_observer.h"
#include "base/task/responsiveness/responsiveness_calculator_delegate.h"

namespace base {
namespace responsiveness {

Watcher::Metadata::Metadata(uintptr_t identifier,
                            bool was_blocked_or_low_priority,
                            base::TimeTicks execution_start_time)
    : identifier(identifier),
      was_blocked_or_low_priority(was_blocked_or_low_priority),
      execution_start_time(execution_start_time) {}

std::unique_ptr<Calculator> Watcher::CreateCalculator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  return std::make_unique<Calculator>(
      std::move(delegate_), histogram_prefix_,
      Calculator::Config{
          .main_source_name = main_source_name_,
          .consume_secondary_sources = consume_secondary_sources_,
          .observe_application_status = observe_application_status_});
}

Watcher::ThreadObserver::ThreadObserver(Calculator* calculator,
                                        bool register_message_loop_observer)
    : ThreadObserver(calculator,
                     /*secondary_sources=*/nullptr,
                     SecondarySources::SourceId(),
                     register_message_loop_observer) {
  CHECK(calculator_);
}

Watcher::ThreadObserver::ThreadObserver(
    SecondarySources* secondary_sources,
    SecondarySources::SourceId secondary_source_id,
    bool register_message_loop_observer)
    : ThreadObserver(/*calculator=*/nullptr,
                     secondary_sources,
                     secondary_source_id,
                     register_message_loop_observer) {
  CHECK(secondary_sources_);
}

Watcher::ThreadObserver::ThreadObserver(
    Calculator* calculator,
    SecondarySources* secondary_sources,
    SecondarySources::SourceId secondary_source_id,
    bool register_message_loop_observer)
    : calculator_(calculator),
      secondary_sources_(secondary_sources),
      secondary_source_id_(secondary_source_id) {
  currently_running_metadata_.reserve(5);
  if (register_message_loop_observer) {
    message_loop_observer_ = std::make_unique<MessageLoopObserver>(
        base::BindRepeating(&ThreadObserver::WillRunTask,
                            base::Unretained(this)),
        base::BindRepeating(&ThreadObserver::DidRunTask,
                            base::Unretained(this)));
  }
}

Watcher::ThreadObserver::~ThreadObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void Watcher::ThreadObserver::WillRunTask(const base::PendingTask* task,
                                          bool was_blocked_or_low_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reentrancy should be rare.
  if (!currently_running_metadata_.empty()) [[unlikely]] {
    currently_running_metadata_.back().caused_reentrancy = true;
  }

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  currently_running_metadata_.emplace_back(reinterpret_cast<uintptr_t>(task),
                                           was_blocked_or_low_priority,
                                           execution_start_time);
}

void Watcher::ThreadObserver::DidRunTask(const base::PendingTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Calls to DidRunTask should always be paired with WillRunTask. The only time
  // the identifier should differ is when the observer is first constructed
  // while a task is already running, which means there was no corresponding
  // WillRunTask.
  if (currently_running_metadata_.empty() ||
      (reinterpret_cast<uintptr_t>(task) !=
       currently_running_metadata_.back().identifier)) [[unlikely]] {
    mismatched_task_identifiers_ += 1;
    // Mismatches can happen, so just ignore them for now. See
    // https://crbug.com/929813 and https://crbug.com/931874 for details.
    return currently_running_metadata_.clear();
  }

  const Metadata metadata = currently_running_metadata_.back();
  currently_running_metadata_.pop_back();

  // Ignore tasks that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (metadata.caused_reentrancy) [[unlikely]] {
    return;
  }

  // Immediate tasks which were posted before the MessageLoopObserver was
  // created will not have a queue_time nor a delayed run time, and should be
  // ignored.
  if (task->queue_time.is_null() && task->delayed_run_time.is_null())
      [[unlikely]] {
    return;
  }

  // For delayed tasks and tasks that were blocked or low priority, pretend that
  // the queuing duration is zero. It is normal to have long queueing time for
  // these tasks, so it shouldn't be used to measure jank.
  const bool is_delayed_task = !task->delayed_run_time.is_null();
  const base::TimeTicks queue_time =
      is_delayed_task || metadata.was_blocked_or_low_priority
          ? metadata.execution_start_time
          : task->queue_time;
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  // TODO(crbug.com/580200095): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK(!queue_time.is_null());
  DCHECK(!metadata.execution_start_time.is_null());
  DCHECK(!execution_finish_time.is_null());
  // TODO(crbug.com/563467054): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK_LE(queue_time, metadata.execution_start_time);
  // TODO(crbug.com/563467054): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK_LE(metadata.execution_start_time, execution_finish_time);

  RecordTaskOrEventFinished(queue_time, metadata.execution_start_time,
                            execution_finish_time);
}

void Watcher::ThreadObserver::WillRunEvent(uintptr_t opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reentrancy should be rare.
  if (!currently_running_metadata_.empty()) [[unlikely]] {
    currently_running_metadata_.back().caused_reentrancy = true;
  }

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  currently_running_metadata_.emplace_back(
      opaque_identifier, /*was_blocked_or_low_priority=*/false,
      execution_start_time);
}

void Watcher::ThreadObserver::DidRunEvent(uintptr_t opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Calls to DidRunEvent should always be paired with WillRunEvent. The only
  // time the identifier should differ is when the observer is first constructed
  // during an event execution.
  if (currently_running_metadata_.empty() ||
      (opaque_identifier != currently_running_metadata_.back().identifier))
      [[unlikely]] {
    mismatched_event_identifiers_ += 1;
    // See comment in DidRunTask() for why `currently_running_metadata_` may be
    // reset.
    return currently_running_metadata_.clear();
  }

  const bool caused_reentrancy =
      currently_running_metadata_.back().caused_reentrancy;
  const base::TimeTicks execution_start_time =
      currently_running_metadata_.back().execution_start_time;
  currently_running_metadata_.pop_back();

  // Ignore events that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (caused_reentrancy) [[unlikely]] {
    return;
  }

  const base::TimeTicks queue_time = execution_start_time;
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  RecordTaskOrEventFinished(queue_time, execution_start_time,
                            execution_finish_time);
}

void Watcher::ThreadObserver::RecordTaskOrEventFinished(
    base::TimeTicks queue_time,
    base::TimeTicks execution_start_time,
    base::TimeTicks execution_finish_time) {
  if (secondary_sources_) {
    secondary_sources_->RecordTaskOrEvent(secondary_source_id_, queue_time,
                                          execution_start_time,
                                          execution_finish_time);
  } else {
    calculator_->TaskOrEventFinishedOnMainThread(
        queue_time, execution_start_time, execution_finish_time);
  }
}

bool Watcher::ShouldRegisterMessageLoopObserver() const {
  return true;
}

void Watcher::WillRunEventOnMainThread(uintptr_t opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  main_thread_observer_->WillRunEvent(opaque_identifier);
}

void Watcher::DidRunEventOnMainThread(uintptr_t opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  main_thread_observer_->DidRunEvent(opaque_identifier);
}

Watcher::Watcher(std::string_view histogram_prefix)
    : Watcher(histogram_prefix, Config{}) {}

Watcher::Watcher(std::string_view histogram_prefix, Config config)
    : histogram_prefix_(histogram_prefix),
      main_source_name_(config.main_source_name),
      consume_secondary_sources_(config.consume_secondary_sources),
      register_first_idle_callback_(config.register_first_idle_callback),
      observe_application_status_(config.observe_application_status),
      delegate_(std::move(config.delegate)) {}

Watcher::~Watcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  idle_callback_subscription_ = {};
  main_thread_observer_.reset();
  calculator_.reset();
}

void Watcher::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  CHECK(!calculator_);

  calculator_ = CreateCalculator();
  main_thread_observer_ = std::make_unique<ThreadObserver>(
      calculator_.get(), ShouldRegisterMessageLoopObserver());

  if (register_first_idle_callback_) {
    idle_callback_subscription_ =
        CurrentThread::Get()->RegisterOnNextIdleCallback(
            CurrentThread::RegisterOnNextIdleCallbackPasskey(),
            base::BindOnce(&Watcher::OnFirstIdle, base::Unretained(this)));
  }
}

// static
void Watcher::EnableForProcess() {
  static base::NoDestructor<SecondarySources> secondary_sources;
}

// static
ScopedClosureRunner Watcher::RegisterCurrentThread(
    std::string_view source_name) {
  SecondarySources* secondary_sources = SecondarySources::Get();
  if (!secondary_sources) {
    return ScopedClosureRunner();
  }
  const SecondarySources::SourceId source_id =
      secondary_sources->RegisterSource(source_name);
  auto observer = std::make_unique<ThreadObserver>(
      secondary_sources, source_id, /*register_message_loop_observer=*/true);
  return ScopedClosureRunner(base::BindOnce(
      [](std::unique_ptr<ThreadObserver>) {}, std::move(observer)));
}

void Watcher::OnFirstIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  calculator_->OnFirstIdle();
}

}  // namespace responsiveness
}  // namespace base
