// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_test_util.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

namespace reporting {

FatalCrashEventsObserver::TestEnvironment::TestEnvironment() = default;
FatalCrashEventsObserver::TestEnvironment::~TestEnvironment() = default;

std::unique_ptr<FatalCrashEventsObserver>
FatalCrashEventsObserver::TestEnvironment::CreateFatalCrashEventsObserver()
    const {
  return base::WrapUnique(new FatalCrashEventsObserver(save_file_path_));
}

const base::FilePath&
FatalCrashEventsObserver::TestEnvironment::GetSaveFilePath() const {
  return save_file_path_;
}

// static
void FatalCrashEventsObserver::TestEnvironment::
    SetInterruptedAfterEventObserved(FatalCrashEventsObserver& observer,
                                     bool interrupted_after_event_observed) {
  observer.SetInterruptedAfterEventObservedForTest(
      interrupted_after_event_observed);
}
}  // namespace reporting
