// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/responsiveness/watcher.h"

#include <utility>
#include <variant>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/pending_task.h"
#include "base/power_monitor/power_monitor.h"
#include "base/task/responsiveness/calculator.h"
#include "base/task/responsiveness/message_loop_observer.h"
#include "base/task/responsiveness/responsiveness_calculator_delegate.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"

namespace base {
namespace responsiveness {

Watcher::Metadata::Metadata(uintptr_t identifier,
                            bool was_blocked_or_low_priority,
                            base::TimeTicks execution_start_time)
    : identifier(identifier),
      was_blocked_or_low_priority(was_blocked_or_low_priority),
      execution_start_time(execution_start_time) {}

std::unique_ptr<Calculator> Watcher::CreateCalculator() {
  return std::make_unique<Calculator>(nullptr);
}

void Watcher::RegisterMessageLoopObserverUI() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  message_loop_observer_ui_ = std::make_unique<MessageLoopObserver>(
      base::BindRepeating(&Watcher::WillRunTaskOnUIThread,
                          base::Unretained(this)),
      base::BindRepeating(&Watcher::DidRunTaskOnUIThread,
                          base::Unretained(this)));
}

void Watcher::RegisterMessageLoopObserverIO() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  message_loop_observer_io_ = std::make_unique<MessageLoopObserver>(
      base::BindRepeating(&Watcher::WillRunTaskOnIOThread,
                          base::Unretained(this)),
      base::BindRepeating(&Watcher::DidRunTaskOnIOThread,
                          base::Unretained(this)));
}

void Watcher::WillRunTaskOnUIThread(const base::PendingTask* task,
                                    bool was_blocked_or_low_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  WillRunTask(task, was_blocked_or_low_priority,
              &currently_running_metadata_ui_);
}

void Watcher::DidRunTaskOnUIThread(const base::PendingTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Capturing `this` is safe because the callback is invoked synchronously by
  // `DidRunTask()`.
  auto lambda = [this](base::TimeTicks queue_time,
                       base::TimeTicks execution_start_time,
                       base::TimeTicks execution_finish_time) {
    calculator_->TaskOrEventFinishedOnUIThread(queue_time, execution_start_time,
                                               execution_finish_time);
  };
  DidRunTask(task, &currently_running_metadata_ui_,
             &mismatched_task_identifiers_ui_, lambda);
}

void Watcher::WillRunTaskOnIOThread(const base::PendingTask* task,
                                    bool was_blocked_or_low_priority) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);

  WillRunTask(task, was_blocked_or_low_priority,
              &currently_running_metadata_io_);
}

void Watcher::DidRunTaskOnIOThread(const base::PendingTask* task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);

  // Capturing `this` is safe because the callback is invoked synchronously by
  // `DidRunTask()`.
  auto lambda = [this](base::TimeTicks queue_time,
                       base::TimeTicks execution_start_time,
                       base::TimeTicks execution_finish_time) {
    calculator_io_->TaskOrEventFinishedOnIOThread(
        queue_time, execution_start_time, execution_finish_time);
  };
  DidRunTask(task, &currently_running_metadata_io_,
             &mismatched_task_identifiers_io_, lambda);
}

void Watcher::WillRunTask(const base::PendingTask* task,
                          bool was_blocked_or_low_priority,
                          std::vector<Metadata>* currently_running_metadata) {
  // Reentrancy should be rare.
  if (!currently_running_metadata->empty()) [[unlikely]] {
    currently_running_metadata->back().caused_reentrancy = true;
  }

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  currently_running_metadata->emplace_back(reinterpret_cast<uintptr_t>(task),
                                           was_blocked_or_low_priority,
                                           execution_start_time);
}

void Watcher::DidRunTask(const base::PendingTask* task,
                         std::vector<Metadata>* currently_running_metadata,
                         int* mismatched_task_identifiers,
                         TaskOrEventFinishedCallback callback) {
  // Calls to DidRunTask should always be paired with WillRunTask. The only time
  // the identifier should differ is when Watcher is first constructed. The
  // TaskRunner Observers may be added while a task is being run, which means
  // that there was no corresponding WillRunTask.
  if (currently_running_metadata->empty() ||
      (reinterpret_cast<uintptr_t>(task) !=
       currently_running_metadata->back().identifier)) [[unlikely]] {
    *mismatched_task_identifiers += 1;
    // Mismatches can happen, so just ignore them for now. See
    // https://crbug.com/929813 and https://crbug.com/931874 for details.
    return currently_running_metadata->clear();
  }

  const Metadata metadata = currently_running_metadata->back();
  currently_running_metadata->pop_back();

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

  CHECK(!queue_time.is_null(), base::NotFatalUntil::M160);
  CHECK(!metadata.execution_start_time.is_null(), base::NotFatalUntil::M160);
  CHECK(!execution_finish_time.is_null(), base::NotFatalUntil::M160);
  // TODO(crbug.com/563467054): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK_LE(queue_time, metadata.execution_start_time);
  // TODO(crbug.com/563467054): CHECK-exclusion: Convert to a CHECK once we are
  // confident it won't be triggered.
  DCHECK_LE(metadata.execution_start_time, execution_finish_time);

  callback(queue_time, metadata.execution_start_time, execution_finish_time);
}

void Watcher::WillRunEventOnUIThread(uintptr_t opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  // Reentrancy should be rare.
  if (!currently_running_metadata_ui_.empty()) [[unlikely]] {
    currently_running_metadata_ui_.back().caused_reentrancy = true;
  }

  const base::TimeTicks execution_start_time = base::TimeTicks::Now();
  currently_running_metadata_ui_.emplace_back(
      opaque_identifier, /* was_blocked_or_low_priority= */ false,
      execution_start_time);
}

void Watcher::DidRunEventOnUIThread(uintptr_t opaque_identifier) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Calls to DidRunEventOnUIThread should always be paired with
  // WillRunEventOnUIThread. The only time the identifier should differ is when
  // Watcher is first constructed. The TaskRunner Observers may be added while a
  // task is being run, which means that there was no corresponding WillRunTask.
  if (currently_running_metadata_ui_.empty() ||
      (opaque_identifier != currently_running_metadata_ui_.back().identifier))
      [[unlikely]] {
    mismatched_event_identifiers_ui_ += 1;
    // See comment in DidRunTask() for why |currently_running_metadata_ui_| may
    // be reset.
    return currently_running_metadata_ui_.clear();
  }

  const bool caused_reentrancy =
      currently_running_metadata_ui_.back().caused_reentrancy;
  const base::TimeTicks execution_start_time =
      currently_running_metadata_ui_.back().execution_start_time;
  currently_running_metadata_ui_.pop_back();

  // Ignore events that caused reentrancy, since their execution latency will
  // be very large, but Chrome was still responsive.
  if (caused_reentrancy) [[unlikely]] {
    return;
  }

  const base::TimeTicks queue_time = execution_start_time;
  const base::TimeTicks execution_finish_time = base::TimeTicks::Now();
  calculator_->TaskOrEventFinishedOnUIThread(queue_time, execution_start_time,
                                             execution_finish_time);
}

Watcher::Watcher(scoped_refptr<SequencedTaskRunner> io_task_runner)
    : io_task_runner_(std::move(io_task_runner)) {
  DETACH_FROM_SEQUENCE(io_sequence_checker_);
}

Watcher::~Watcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void Watcher::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);

  // Set up |calculator_| before SetUpOnIOThread() uses |calculator_|.
  calculator_ = CreateCalculator();
  currently_running_metadata_ui_.reserve(5);

  RegisterMessageLoopObserverUI();
  io_task_runner_->PostTask(FROM_HERE, base::BindOnce(&Watcher::SetUpOnIOThread,
                                                      base::Unretained(this)));
}

void Watcher::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
  CHECK(!destroy_was_called_, base::NotFatalUntil::M160);
  destroy_was_called_ = true;

  message_loop_observer_ui_.reset();

  // `base::Unretained(this)` is safe because the reply callback holds a
  // reference via `base::RetainedRef(this)`, keeping `this` alive across both
  // `TearDownOnIOThread()` and `TearDownOnUIThread()`.
  io_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Watcher::TearDownOnIOThread, base::Unretained(this)),
      base::BindOnce(&Watcher::TearDownOnUIThread, base::RetainedRef(this)));
}

void Watcher::OnFirstIdle() {
  calculator_->OnFirstIdle();
}

void Watcher::SetUpOnIOThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  currently_running_metadata_io_.reserve(5);
  CHECK(calculator_.get(), base::NotFatalUntil::M160);
  calculator_io_ = calculator_.get();
  RegisterMessageLoopObserverIO();
}

void Watcher::TearDownOnUIThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
}

void Watcher::TearDownOnIOThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(io_sequence_checker_);
  message_loop_observer_io_.reset();
  calculator_io_ = nullptr;
}

}  // namespace responsiveness
}  // namespace base
