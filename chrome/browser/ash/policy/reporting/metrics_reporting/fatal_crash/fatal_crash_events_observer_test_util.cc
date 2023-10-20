// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_test_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

namespace reporting {

FatalCrashEventsObserver::TestEnvironment::TestEnvironment() = default;
FatalCrashEventsObserver::TestEnvironment::~TestEnvironment() = default;

std::unique_ptr<FatalCrashEventsObserver>
FatalCrashEventsObserver::TestEnvironment::CreateFatalCrashEventsObserver()
    const {
  return base::WrapUnique(new FatalCrashEventsObserver(
      GetReportedLocalIdSaveFilePath(), GetUploadedCrashInfoSaveFilePath()));
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

}  // namespace reporting
