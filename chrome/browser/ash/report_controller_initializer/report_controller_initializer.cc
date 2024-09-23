// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/report_controller_initializer/report_controller_initializer.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/report/device_metrics/use_case/real_psm_client_manager.h"
#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"
#include "chromeos/ash/components/report/utils/time_utils.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

// Path to file storing the last powerwash time, persisted over safe powerwash.
constexpr char kLastPowerwashTimePath[] =
    "/mnt/stateful_partition/unencrypted/preserve/last_powerwash_time";

// Number of minutes to wait before retrying
// reading the .oobe_completed file again.
constexpr base::TimeDelta kOobeReadFailedRetryDelay = base::Minutes(60);

// Number of times to retry before failing to report any device actives.
constexpr int kNumberOfRetriesBeforeFail = 120;

// Record the state transitions for the |ReportInitializer| class.
void RecordInitializerState(ReportControllerInitializer::State state) {
  base::UmaHistogramEnumeration("Ash.Report.InitializerState", state);
}

// Record minutes of startup delay before reporting.
void RecordStartupDelay(int delay_minutes) {
  base::UmaHistogramCustomCounts("Ash.Report.StartupDelay", delay_minutes,
                                 /*min=*/0,
                                 /*exclusive_max=*/60, /*buckets=*/61);
}

// Record whether oobe is completed.
void RecordIsOobeCompleted(bool is_complete) {
  base::UmaHistogramBoolean("Ash.Report.IsOobeCompleted", is_complete);
}

void RecordLastPowerwashTimeRead(bool success) {
  base::UmaHistogramBoolean("Ash.Report.IsLastPowerwashTimeRead", success);
}

// Record the device trusted status enum when checking policy trusted status.
void RecordTrustedStatus(CrosSettingsProvider::TrustedStatus status) {
  ReportControllerInitializer::TrustedStatus status_mapped;
  switch (status) {
    case CrosSettingsProvider::TrustedStatus::PERMANENTLY_UNTRUSTED:
      status_mapped =
          ReportControllerInitializer::TrustedStatus::kPermanentlyUntrusted;
      break;
    case CrosSettingsProvider::TrustedStatus::TEMPORARILY_UNTRUSTED:
      status_mapped =
          ReportControllerInitializer::TrustedStatus::kTemporarilyUntrusted;
      break;
    case CrosSettingsProvider::TrustedStatus::TRUSTED:
      status_mapped = ReportControllerInitializer::TrustedStatus::kTrusted;
      break;
  }

  base::UmaHistogramEnumeration("Ash.Report.TrustedStatus", status_mapped);
}

// Record the device market segment after oobe completed and segment is ready.
// @param market_segment Defined in fresnel_service.proto
void RecordMarketSegment(report::MarketSegment market_segment) {
  ReportControllerInitializer::MarketSegment market_segment_mapped;
  switch (market_segment) {
    case report::MarketSegment::MARKET_SEGMENT_UNSPECIFIED:
      market_segment_mapped =
          ReportControllerInitializer::MarketSegment::kUnspecified;
      break;
    case report::MarketSegment::MARKET_SEGMENT_UNKNOWN:
      market_segment_mapped =
          ReportControllerInitializer::MarketSegment::kUnspecified;
      break;
    case report::MarketSegment::MARKET_SEGMENT_CONSUMER:
      market_segment_mapped =
          ReportControllerInitializer::MarketSegment::kConsumer;
      break;
    case report::MarketSegment::MARKET_SEGMENT_ENTERPRISE_ENROLLED_BUT_UNKNOWN:
      market_segment_mapped = ReportControllerInitializer::MarketSegment::
          kEnterpriseEnrolledButUnknown;
      break;
    case report::MarketSegment::MARKET_SEGMENT_ENTERPRISE:
      market_segment_mapped =
          ReportControllerInitializer::MarketSegment::kEnterprise;
      break;
    case report::MarketSegment::MARKET_SEGMENT_EDUCATION:
      market_segment_mapped =
          ReportControllerInitializer::MarketSegment::kEducation;
      break;
    case report::MarketSegment::MARKET_SEGMENT_ENTERPRISE_DEMO:
      market_segment_mapped =
          ReportControllerInitializer::MarketSegment::kEnterpriseDemo;
      break;
  }

  base::UmaHistogramEnumeration("Ash.Report.MarketSegment",
                                market_segment_mapped);
}

// Determine market segment from the loaded ChromeOS device policies.
report::MarketSegment GetMarketSegment(
    policy::DeviceMode device_mode,
    policy::MarketSegment device_market_segment) {
  // Policy device modes that should be classified as not being set.
  const std::unordered_set<policy::DeviceMode> kDeviceModeNotSet{
      policy::DeviceMode::DEVICE_MODE_PENDING,
      policy::DeviceMode::DEVICE_MODE_NOT_SET};

  // Policy device modes that should be classified as consumer devices.
  const std::unordered_set<policy::DeviceMode> kDeviceModeConsumer{
      policy::DeviceMode::DEVICE_MODE_CONSUMER};

  // Policy device modes that should be classified as enterprise devices.
  const std::unordered_set<policy::DeviceMode> kDeviceModeEnterprise{
      policy::DeviceMode::DEVICE_MODE_ENTERPRISE};

  // Policy device modes that should be classified as demo devices.
  const std::unordered_set<policy::DeviceMode> kDeviceModeDemoEnterprise{
      policy::DeviceMode::DEVICE_MODE_DEMO};

  // Determine Fresnel market segment using the retrieved device policy
  // |device_mode| and |device_market_segment|.
  if (kDeviceModeNotSet.count(device_mode)) {
    return report::MARKET_SEGMENT_UNKNOWN;
  }

  if (kDeviceModeConsumer.count(device_mode)) {
    return report::MARKET_SEGMENT_CONSUMER;
  }

  if (kDeviceModeDemoEnterprise.count(device_mode)) {
    return report::MARKET_SEGMENT_ENTERPRISE_DEMO;
  }

  if (kDeviceModeEnterprise.count(device_mode)) {
    if (device_market_segment == policy::MarketSegment::ENTERPRISE) {
      return report::MARKET_SEGMENT_ENTERPRISE;
    }

    if (device_market_segment == policy::MarketSegment::EDUCATION) {
      return report::MARKET_SEGMENT_EDUCATION;
    }

    return report::MARKET_SEGMENT_ENTERPRISE_ENROLLED_BUT_UNKNOWN;
  }

  return report::MARKET_SEGMENT_UNKNOWN;
}

// Reads the last powerwash time from preserved files. If the device is new
// or the last powerwash time file does not exist, it will return UnixEpoch.
base::Time ReadLastPowerwashTime() {
  // Retrieve the last modified time of the powerwash time file.
  base::FilePath last_powerwash_file(kLastPowerwashTimePath);
  base::File::Info info;
  if (!base::GetFileInfo(last_powerwash_file, &info)) {
    LOG(ERROR) << "Failed to get last powerwash file info.";
    return base::Time::UnixEpoch();
  }

  return info.last_modified;
}

}  // namespace

ReportControllerInitializer::ReportControllerInitializer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SetState(State::kWaitingForOwnership);

  // Adds observer for device ownership status changes in this class.
  device_settings_observation_.Observe(DeviceSettingsService::Get());

  OwnershipStatusChanged();
}

ReportControllerInitializer::~ReportControllerInitializer() = default;

void ReportControllerInitializer::SetState(State state) {
  state_ = state;
  RecordInitializerState(state_);
}

report::MarketSegment ReportControllerInitializer::GetMarketSegmentForTesting(
    const policy::DeviceMode& device_mode,
    const policy::MarketSegment& device_market_segment) {
  return GetMarketSegment(device_mode, device_market_segment);
}

void ReportControllerInitializer::OwnershipStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (state_ != State::kWaitingForOwnership) {
    LOG(ERROR) << "Invalid state - expected to be waiting for ownership.";
    return;
  }

  // Device should only get ownership taken at most once on a browser start up.
  if (ash::DeviceSettingsService::Get()->GetOwnershipStatus() !=
      ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken) {
    LOG(ERROR) << "Ownership status is not taken yet, returning early.";
    return;
  }

  SetState(State::kWaitingForStartupDelay);

  // Retrieve chrome first run sentinel time.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&first_run::GetFirstRunSentinelCreationTime),
      base::BindOnce(
          &ReportControllerInitializer::OnFirstRunSentinelCreationTimeRead,
          weak_factory_.GetWeakPtr()));
}

void ReportControllerInitializer::OnFirstRunSentinelCreationTimeRead(
    base::Time first_chrome_run_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForStartupDelay);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &ReportControllerInitializer::CheckOobeCompleted,
          weak_factory_.GetWeakPtr(),
          base::BindRepeating(&StartupUtils::GetTimeSinceOobeFlagFileCreation)),
      DetermineStartUpDelay(first_chrome_run_time));
}

base::TimeDelta ReportControllerInitializer::DetermineStartUpDelay(
    base::Time chrome_first_run_ts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForStartupDelay);

  // Wait at least 1 hour from the first chrome run sentinel file creation
  // time. This creation time is used as an indicator of when the device last
  // reset (powerwash/recovery/RMA). PSM servers can take 1 hour after CheckIn
  // to return the correct response for CheckMembership requests, since the PSM
  // servers need to update their cache.
  //
  // This delay avoids the scenario where a device checks in, powerwashes, and
  // on device start up, gets the wrong check membership response.
  base::TimeDelta delay_on_first_chrome_run;
  base::Time current_ts = base::Time::Now();
  if (current_ts < (chrome_first_run_ts + base::Hours(1))) {
    delay_on_first_chrome_run =
        chrome_first_run_ts + base::Hours(1) - current_ts;
  }

  RecordStartupDelay(delay_on_first_chrome_run.InMinutes());
  return delay_on_first_chrome_run;
}

void ReportControllerInitializer::CheckOobeCompleted(
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback) {
  SetState(State::kWaitingForOobeCompleted);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We block if the oobe completed file is not written.
  // ChromeOS devices should go through oobe to be considered a real device.
  // The ActivateDate is also only set after oobe is written.
  if (retry_oobe_completed_count_ >= kNumberOfRetriesBeforeFail) {
    LOG(ERROR) << "Retry failed - .oobe_completed file was not written for "
               << "1 minute after retrying 120 times. "
               << "There was a 60 minute wait between each retry and spanned "
               << "5 days.";
    RecordIsOobeCompleted(false);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(check_oobe_completed_callback),
      base::BindOnce(&ReportControllerInitializer::OnOobeFileWritten,
                     weak_factory_.GetWeakPtr(),
                     check_oobe_completed_callback));
}

void ReportControllerInitializer::OnOobeFileWritten(
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback,
    base::TimeDelta time_since_oobe_file_written) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForOobeCompleted);

  // If the OOBE completed file isn't created yet,
  // time_since_oobe_file_written returns base::TimeDelta().
  if (time_since_oobe_file_written == base::TimeDelta() ||
      time_since_oobe_file_written < base::Minutes(1)) {
    ++retry_oobe_completed_count_;

    LOG(ERROR) << "Time since oobe file created was less than 1 minute. "
               << "Wait and retry again after 1 minute to ensure that "
               << "the ActivateDate VPD field is set. "
               << "TimeDelta since oobe flag file was created = "
               << time_since_oobe_file_written
               << ". Retry count = " << retry_oobe_completed_count_;

    RecordIsOobeCompleted(false);

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ReportControllerInitializer::CheckOobeCompleted,
                       weak_factory_.GetWeakPtr(),
                       std::move(check_oobe_completed_callback)),
        kOobeReadFailedRetryDelay);

    return;
  }

  RecordIsOobeCompleted(true);

  CheckTrustedStatus();
}

void ReportControllerInitializer::CheckTrustedStatus() {
  SetState(State::kWaitingForDeviceSettingsTrusted);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForDeviceSettingsTrusted);

  // Device is owned, confirm the settings can be trusted.
  CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&ReportControllerInitializer::CheckTrustedStatus,
                         weak_factory_.GetWeakPtr()));

  // Record histogram that indicates the status of the device policies.
  RecordTrustedStatus(status);

  if (status == CrosSettingsProvider::TEMPORARILY_UNTRUSTED ||
      status == CrosSettingsProvider::PERMANENTLY_UNTRUSTED) {
    // When status is TEMPORARILY_UNTRUSTED, PrepareTrustedValues method takes
    // ownership of the start report controller callback.
    // It will retry later when the TRUSTED status becomes available.
    //
    // When status is PERMANENTLY_UNTRUSTED, client assumes this status is final
    // until browser restarts. Client does not proceed without signature
    // verification, so retry is not attempted. This status may be caused
    // if the policy proto blob fails the signature check.
    LOG(ERROR) << "CrosSettings status is not trusted yet.";
    return;
  }

  SetState(State::kWaitingForLastPowerwashTime);

  // Retrieve last powerwash time, if file exists.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadLastPowerwashTime),
      base::BindOnce(&ReportControllerInitializer::OnLastPowerwashTimeRead,
                     weak_factory_.GetWeakPtr()));
}

void ReportControllerInitializer::OnLastPowerwashTimeRead(
    base::Time last_powerwash_gmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForLastPowerwashTime);

  // Default values before handling last powerwash time.
  // Variable is based off GMT YYYY-WW just like ActivateDate VPD field.
  std::string last_powerwash_week;
  // Handle the last powerwash time received after read attempt.
  if (last_powerwash_gmt.is_null() ||
      last_powerwash_gmt == base::Time::UnixEpoch()) {
    RecordLastPowerwashTimeRead(false);
  } else {
    last_powerwash_week =
        report::utils::ConvertTimeToISO8601String(last_powerwash_gmt);
    RecordLastPowerwashTimeRead(true);
  }

  // OOBE is completed, so we can safely calculate the device market
  // segment.
  report::MarketSegment device_market_segment =
      GetMarketSegment(g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetDeviceMode(),
                       g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetEnterpriseMarketSegment());

  // Record histogram after oobe is completed and the policies are in trusted
  // status. At this point, the device market segment is known and assigned.
  RecordMarketSegment(device_market_segment);

  SetState(State::kReportControllerInitialized);

  // At this step we have checked for 3 conditions.
  // 1. The device is owned.
  // 2. OOBE is completed and .oobe_completed file is written > 1 minute ago.
  // 3. CrosSettingsProvider::TRUSTED: device policies are loaded and trusted.
  report_controller_ = std::make_unique<report::ReportController>(
      ash::report::device_metrics::ChromeDeviceMetadataParameters{
          chrome::GetChannel() /* chromeos_channel */, device_market_segment,
          last_powerwash_week},
      g_browser_process->local_state(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      std::make_unique<ash::report::device_metrics::PsmClientManager>(
          std::make_unique<
              report::device_metrics::RealPsmClientManagerDelegate>()));
}

}  // namespace ash
