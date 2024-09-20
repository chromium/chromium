// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_CHROME_FATAL_CRASH_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_CHROME_FATAL_CRASH_EVENTS_OBSERVER_H_

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

// Observes fatal Chrome crash events. The only difference between this class
// and FatalCrashEventsObserver are the paths where reported local IDs and
// uploaded crash info are stored and the crash types that are reported.
class ChromeFatalCrashEventsObserver
    : public reporting::FatalCrashEventsObserver {
 public:
  static std::unique_ptr<ChromeFatalCrashEventsObserver> Create();

 private:
  // Give `TestEnvironment` the access to the private constructor that
  // specifies the path for the save file.
  friend class FatalCrashEventsObserver::TestEnvironment;

  // This constructor enables the test code to use non-default values of the
  // input parameters to accommodate the test environment. In production code,
  // they are always the default value specified in the default constructor.
  ChromeFatalCrashEventsObserver(
      const base::FilePath& reported_local_id_save_file_path,
      const base::FilePath& uploaded_crash_info_save_file_path,
      scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner,
      scoped_refptr<base::SequencedTaskRunner>
          uploaded_crash_info_io_task_runner);

  FatalCrashTelemetry::CrashType GetFatalCrashTelemetryCrashType(
      ::ash::cros_healthd::mojom::CrashEventInfo::CrashType crash_type)
      const override;
  // Get allowed crash types.
  const base::flat_set<::ash::cros_healthd::mojom::CrashEventInfo::CrashType>&
  GetAllowedCrashTypes() const override;
};
}  // namespace reporting
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_CHROME_FATAL_CRASH_EVENTS_OBSERVER_H_
