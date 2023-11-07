// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_test_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_reported_local_id_manager.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_uploaded_crash_info_manager.h"

namespace reporting {

FatalCrashEventsObserver::TestEnvironment::TestEnvironment() = default;
FatalCrashEventsObserver::TestEnvironment::~TestEnvironment() = default;

std::unique_ptr<FatalCrashEventsObserver>
FatalCrashEventsObserver::TestEnvironment::CreateFatalCrashEventsObserver(
    scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner)
    const {
  auto observer = base::WrapUnique(new FatalCrashEventsObserver(
      GetReportedLocalIdSaveFilePath(), GetUploadedCrashInfoSaveFilePath(),
      reported_local_id_io_task_runner));

  if (reported_local_id_io_task_runner == nullptr) {
    // For most tests, we focus on the behavior after save files are loaded.
    // Thus, make sure IO is completed to prevent flaky tests.
    FlushIoTasks(*observer);
  }

  // Clear tasks such as registering the observer.
  base::RunLoop().RunUntilIdle();
  return observer;
}

const base::FilePath&
FatalCrashEventsObserver::TestEnvironment::GetReportedLocalIdSaveFilePath()
    const {
  return reported_local_id_save_file_path_;
}

const base::FilePath&
FatalCrashEventsObserver::TestEnvironment::GetUploadedCrashInfoSaveFilePath()
    const {
  return uploaded_crash_info_save_file_path_;
}

// static
void FatalCrashEventsObserver::TestEnvironment::
    SetInterruptedAfterEventObserved(FatalCrashEventsObserver& observer,
                                     bool interrupted_after_event_observed) {
  observer.SetInterruptedAfterEventObservedForTest(
      interrupted_after_event_observed);
}

// static
size_t FatalCrashEventsObserver::TestEnvironment::GetLocalIdEntryQueueSize(
    FatalCrashEventsObserver& observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer.sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      observer.reported_local_id_manager_->sequence_checker_);
  return observer.reported_local_id_manager_->local_id_entry_queue_.size();
}

// static
void FatalCrashEventsObserver::TestEnvironment::FlushIoTasks(
    FatalCrashEventsObserver& observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(observer.sequence_checker_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(
      observer.reported_local_id_manager_->sequence_checker_);

  // Block the main thread while flushing IO tasks. Not using `PostTaskAndReply`
  // on QuitClosure because the QuitClosure task must be posted first before the
  // main thread can be unblocked to prevent race.
  SequenceBlocker sequence_blocker(
      base::SequencedTaskRunner::GetCurrentDefault());
  base::RunLoop run_loop;
  observer.reported_local_id_manager_->io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](scoped_refptr<base::SequencedTaskRunner> main_task_runner,
             SequenceBlocker* sequence_blocker,
             base::RepeatingClosure quit_closure) {
            main_task_runner->PostTask(FROM_HERE, std::move(quit_closure));
            sequence_blocker->Unblock();
          },
          base::SequencedTaskRunner::GetCurrentDefault(),
          // Safe to pass the address of sequence_blocker because run_loop.Run()
          // below will clear the task posted by the blocker.
          base::Unretained(&sequence_blocker), run_loop.QuitClosure()));
  run_loop.Run();
}

FatalCrashEventsObserver::TestEnvironment::SequenceBlocker::SequenceBlocker(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  task_runner->PostTask(FROM_HERE, base::BindOnce(
                                       [](std::atomic<bool>* blocked) {
                                         while (blocked->load()) {
                                           // Wait...
                                         }
                                       },
                                       &blocked_));
}

void FatalCrashEventsObserver::TestEnvironment::SequenceBlocker::Unblock() {
  blocked_ = false;
}
}  // namespace reporting
