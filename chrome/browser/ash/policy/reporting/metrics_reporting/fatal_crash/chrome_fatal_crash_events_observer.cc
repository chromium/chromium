// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/chrome_fatal_crash_events_observer.h"

#include <base/no_destructor.h>

#include "base/files/file_path.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

using ::ash::cros_healthd::mojom::CrashEventInfo;

namespace reporting {

constexpr base::FilePath::StringPieceType kChromeReportedLocalIdSaveFilePath =
    "/var/lib/reporting/crash_events/CHROME_CRASH_REPORTED_LOCAL_IDS";
constexpr base::FilePath::StringPieceType kChromeUploadedCrashInfoSaveFilePath =
    "/var/lib/reporting/crash_events/CHROME_CRASH_UPLOADED_CRASH_INFO";

ChromeFatalCrashEventsObserver::ChromeFatalCrashEventsObserver(
    const base::FilePath& reported_local_id_save_file_path,
    const base::FilePath& uploaded_crash_info_save_file_path,
    scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner,
    scoped_refptr<base::SequencedTaskRunner> uploaded_crash_info_io_task_runner)
    : reporting::FatalCrashEventsObserver(reported_local_id_save_file_path,
                                          uploaded_crash_info_save_file_path,
                                          reported_local_id_io_task_runner,
                                          uploaded_crash_info_io_task_runner) {}

// static
std::unique_ptr<ChromeFatalCrashEventsObserver>
ChromeFatalCrashEventsObserver::Create() {
  return base::WrapUnique(new ChromeFatalCrashEventsObserver(
      base::FilePath(kChromeReportedLocalIdSaveFilePath),
      base::FilePath(kChromeUploadedCrashInfoSaveFilePath),
      /*reported_local_id_io_task_runner=*/nullptr,
      /*uploaded_crash_info_io_task_runner=*/nullptr));
}

FatalCrashTelemetry::CrashType
ChromeFatalCrashEventsObserver::GetFatalCrashTelemetryCrashType(
    CrashEventInfo::CrashType crash_type) const {
  switch (crash_type) {
    case CrashEventInfo::CrashType::kChrome:
      return FatalCrashTelemetry::CRASH_TYPE_CHROME;
    case CrashEventInfo::CrashType::kUnknown:
      [[fallthrough]];
    default:  // Other types added by healthD that are unknown here yet.
      NOTREACHED() << "Encountered unhandled or unknown crash type "
                   << crash_type;
  }
}

const base::flat_set<CrashEventInfo::CrashType>&
ChromeFatalCrashEventsObserver::GetAllowedCrashTypes() const {
  static const base::NoDestructor<base::flat_set<CrashEventInfo::CrashType>>
      allowed_crash_types({CrashEventInfo::CrashType::kChrome});
  return *allowed_crash_types;
}

}  // namespace reporting
