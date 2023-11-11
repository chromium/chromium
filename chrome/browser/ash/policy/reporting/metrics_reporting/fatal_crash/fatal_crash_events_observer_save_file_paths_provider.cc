// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer_save_file_paths_provider.h"

#include "base/files/file_path.h"

namespace reporting {

FatalCrashEventsObserver::DefaultSaveFilePathsProvider::
    DefaultSaveFilePathsProvider() = default;
FatalCrashEventsObserver::DefaultSaveFilePathsProvider::
    ~DefaultSaveFilePathsProvider() = default;

// static
const FatalCrashEventsObserver::SaveFilePathsProviderInterface*
    FatalCrashEventsObserver::SaveFilePathsProviderInterface::
        g_default_save_file_paths_provider_{nullptr};

// static
const FatalCrashEventsObserver::SaveFilePathsProviderInterface&
FatalCrashEventsObserver::DefaultSaveFilePathsProvider::Get() {
  if (!g_default_save_file_paths_provider_) {
    g_default_save_file_paths_provider_ = new DefaultSaveFilePathsProvider();
  }
  return *g_default_save_file_paths_provider_;
}

base::FilePath FatalCrashEventsObserver::DefaultSaveFilePathsProvider::
    GetReportedLocalIdSaveFilePath() const {
  return base::FilePath("/var/lib/reporting/crash_events/REPORTED_LOCAL_IDS");
}

base::FilePath FatalCrashEventsObserver::DefaultSaveFilePathsProvider::
    GetUploadedCrashInfoSaveFilePath() const {
  return base::FilePath("/var/lib/reporting/crash_events/UPLOADED_CRASH_INFO");
}
}  // namespace reporting
