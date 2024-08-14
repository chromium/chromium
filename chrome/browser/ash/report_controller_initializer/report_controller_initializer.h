// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REPORT_CONTROLLER_INITIALIZER_REPORT_CONTROLLER_INITIALIZER_H_
#define CHROME_BROWSER_ASH_REPORT_CONTROLLER_INITIALIZER_REPORT_CONTROLLER_INITIALIZER_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/ash/components/report/report_controller.h"

namespace ash {

// Checks that preconditions are met, including device ownership and device
// settings trusted status.
// Then initializes |ReportController|.
class ReportControllerInitializer : public DeviceSettingsService::Observer {
 public:
  // State machine for preconditions this class will be in.
  enum class State {
    kWaitingForOwnership = 0,      // Wait for ownership to be taken.
    kWaitingForStartupDelay = 1,   // Wait to read first chrome run time &
                                   // startup delay to be completed.
    kWaitingForOobeCompleted = 2,  // Wait for oobe completed conditions.
    kWaitingForDeviceSettingsTrusted = 3,  // Wait for policies to be trusted.
    kWaitingForLastPowerwashTime = 4,  // Wait to read last powerwash time if
                                       // file exists in preserved files.
    kReportControllerInitialized = 5,  // Nothing left to do.
    kMaxValue = kReportControllerInitialized,
  };

  // Enum class mapping CrosSettingsProvider::TrustedStatus to an enum class.
  // Used for enumerating TrustedStatus UMA histograms.
  enum class TrustedStatus {
    kTrusted = 0,
    kTemporarilyUntrusted = 1,
    kPermanentlyUntrusted = 2,
    kMaxValue = kPermanentlyUntrusted,
  };

  // Enum class mapping report::MarketSegment to an enum class.
  // Used for enumerating MarketSegment UMA histograms.
  enum class MarketSegment {
    kUnspecified = 0,
    kUnknown = 1,
    kConsumer = 2,
    kEnterpriseEnrolledButUnknown = 3,
    kEnterprise = 4,
    kEducation = 5,
    kEnterpriseDemo = 6,
    kMaxValue = kEnterpriseDemo,
  };

  // Trigger checks for preconditions before construction of |ReportController|.
  ReportControllerInitializer();
  ReportControllerInitializer(const ReportControllerInitializer&) = delete;
  ReportControllerInitializer& operator=(const ReportControllerInitializer&) =
      delete;
  ~ReportControllerInitializer() override;

 private:
  // Grant friend access for comprehensive testing of private/protected members.
  friend class ReportControllerInitializerValidateSegment;

  void SetState(State state);

  // Method is used for testing:
  report::MarketSegment GetMarketSegmentForTesting(
      const policy::DeviceMode& device_mode,
      const policy::MarketSegment& device_market_segment);

  // DeviceSettingsService::Observer:
  void OwnershipStatusChanged() override;

  // Handler after reading first run chrome time in ThreadPool task.
  // This is done to avoid blocking the main browser thread.
  void OnFirstRunSentinelCreationTimeRead(base::Time first_chrome_run_time);

  // Determine start up delay before reporting should start.
  base::TimeDelta DetermineStartUpDelay(base::Time chrome_first_run_ts);

  // Wrapper method for the PostTaskAndReplyWithResult, which is used to spawn
  // a worker thread to check oobe completed file time delta.
  void CheckOobeCompleted(
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback);

  // Retry method every kOobeReadFailedRetryDelay minute until confirming
  // 1 minute has passed since /home/chronos/.oobe_completed file was written.
  // Maximum retry count is kNumberOfRetriesBeforeFail.
  void OnOobeFileWritten(
      base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback,
      base::TimeDelta time_since_oobe_file_written);

  // Determines whether the CrosSettings is trusted.
  // If it is trusted, ReportController is initialized.
  void CheckTrustedStatus();

  // Handler after reading last powerwash time file in ThreadPool task.
  // This is done to avoid blocking the main browser thread.
  void OnLastPowerwashTimeRead(base::Time last_powerwash_time);

  // Store the current state this class is in.
  State state_ = State::kWaitingForOwnership;

  // Number of retry attempts at reading the oobe completed file.
  int retry_oobe_completed_count_ = 0;

  // Class maintains ownership of |report_controller_| after it is initialized.
  std::unique_ptr<report::ReportController> report_controller_;

  // Sanity check to check methods are called in same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  // Scoped observation to automatically manage the observer registration
  // and unregistration with DeviceSettingsService.
  base::ScopedObservation<DeviceSettingsService,
                          DeviceSettingsService::Observer>
      device_settings_observation_{this};

  base::WeakPtrFactory<ReportControllerInitializer> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_REPORT_CONTROLLER_INITIALIZER_REPORT_CONTROLLER_INITIALIZER_H_
