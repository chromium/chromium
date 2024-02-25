// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_SETTINGS_FOR_TEST_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_SETTINGS_FOR_TEST_H_

#include <cstdint>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {
struct FatalCrashEventsObserver::SettingsForTest final {
  using SkippedUninterestingCrashTypeCallback = base::RepeatingCallback<void(
      ash::cros_healthd::mojom::CrashEventInfo::CrashType)>;
  using SkippedUnuploadedCrashCallback =
      base::RepeatingCallback<void(LocalIdEntry)>;
  using SkippedUploadedCrashCallback =
      base::RepeatingCallback<void(std::string /* crash_report_id */,
                                   base::Time /* creation_time */,
                                   uint64_t /* offset */)>;
  using EventCollectedBeforeSaveFilesLoadedCallback =
      base::RepeatingCallback<void(
          ::ash::cros_healthd::mojom::CrashEventInfoPtr)>;

  SettingsForTest();
  SettingsForTest(const SettingsForTest&) = delete;
  SettingsForTest& operator=(const SettingsForTest&) = delete;
  ~SettingsForTest();

  // This is public because this struct is data-only, thus using data members of
  // this struct would require the user to check this sequence checker.
  //
  // Can't make this private and expose a const reference of it via a method
  // because the direct use of `base::SequenceChecker` is banned by presubmit.
  SEQUENCE_CHECKER(sequence_checker);

  // If true, stop the processing after the event observed callback is called.
  // Setting to true to simulate that event observed callback is interrupted
  // right after it's finished.
  bool interrupted_after_event_observed GUARDED_BY_CONTEXT(sequence_checker){
      false};

  // Called when a crash is skipped due to an unintetesting crash type.
  SkippedUninterestingCrashTypeCallback
      skipped_uninteresting_crash_type_callback
          GUARDED_BY_CONTEXT(sequence_checker){base::DoNothing()};

  // Called when an unuploaded crash is skipped and not reported.
  SkippedUnuploadedCrashCallback skipped_unuploaded_crash_callback
      GUARDED_BY_CONTEXT(sequence_checker){base::DoNothing()};

  // Called when an uploaded crash is skipped and not reported.
  SkippedUploadedCrashCallback skipped_uploaded_crash_callback
      GUARDED_BY_CONTEXT(sequence_checker){base::DoNothing()};

  // Called when a crash event is queued due to a delayed save file loading.
  EventCollectedBeforeSaveFilesLoadedCallback
      event_collected_before_save_files_loaded_callback
          GUARDED_BY_CONTEXT(sequence_checker);

 private:
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_SETTINGS_FOR_TEST_H_
