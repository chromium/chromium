// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/report_controller_initializer.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/channel_info.h"
#include "chromeos/ash/components/report/device_metrics/use_case/real_psm_client_manager.h"
#include "chromeos/ash/components/report/proto/fresnel_service.pb.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {

namespace {

// Number of minutes to wait before retrying
// reading the .oobe_completed file again.
constexpr base::TimeDelta kOobeReadFailedRetryDelay = base::Minutes(60);

// Number of times to retry before failing to report any device actives.
constexpr int kNumberOfRetriesBeforeFail = 120;

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
      policy::DeviceMode::DEVICE_MODE_CONSUMER,
      policy::DeviceMode::DEVICE_MODE_CONSUMER_KIOSK_AUTOLAUNCH};

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

}  // namespace

ReportControllerInitializer::ReportControllerInitializer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  state_ = State::kWaitingForOwnership;

  // Adds observer for device ownership status changes in this class.
  device_settings_observation_.Observe(DeviceSettingsService::Get());

  OwnershipStatusChanged();
}

ReportControllerInitializer::~ReportControllerInitializer() = default;

report::MarketSegment ReportControllerInitializer::GetMarketSegmentForTesting(
    const policy::DeviceMode& device_mode,
    const policy::MarketSegment& device_market_segment) {
  return GetMarketSegment(device_mode, device_market_segment);
}

void ReportControllerInitializer::OwnershipStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Device should only get ownership taken at most once on a browser start up.
  if (state_ != State::kWaitingForOwnership) {
    return;
  }

  if (ash::DeviceSettingsService::Get()->GetOwnershipStatus() !=
      ash::DeviceSettingsService::OwnershipStatus::kOwnershipTaken) {
    return;
  }

  state_ = State::kWaitingForStartupDelay;

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &ReportControllerInitializer::CheckOobeCompleted,
          weak_factory_.GetWeakPtr(),
          base::BindRepeating(&StartupUtils::GetTimeSinceOobeFlagFileCreation)),
      DetermineStartUpDelay(first_run::GetFirstRunSentinelCreationTime()));
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

  return delay_on_first_chrome_run;
}

void ReportControllerInitializer::CheckOobeCompleted(
    base::RepeatingCallback<base::TimeDelta()> check_oobe_completed_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForStartupDelay);
  state_ = State::kWaitingForOobeCompleted;

  // We block if the oobe completed file is not written.
  // ChromeOS devices should go through oobe to be considered a real device.
  // The ActivateDate is also only set after oobe is written.
  if (retry_oobe_completed_count_ >= kNumberOfRetriesBeforeFail) {
    LOG(ERROR) << "Retry failed - .oobe_completed file was not written for "
               << "1 minute after retrying 120 times. "
               << "There was a 60 minute wait between each retry and spanned "
               << "5 days.";
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

    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ReportControllerInitializer::CheckOobeCompleted,
                       weak_factory_.GetWeakPtr(),
                       std::move(check_oobe_completed_callback)),
        kOobeReadFailedRetryDelay);

    return;
  }

  state_ = State::kWaitingForDeviceSettingsTrusted;
  CheckTrustedStatus();
}

void ReportControllerInitializer::CheckTrustedStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, State::kWaitingForDeviceSettingsTrusted);

  // Device is owned, confirm the settings can be trusted.
  CrosSettingsProvider::TrustedStatus status =
      CrosSettings::Get()->PrepareTrustedValues(
          base::BindOnce(&ReportControllerInitializer::CheckTrustedStatus,
                         weak_factory_.GetWeakPtr()));

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
    return;
  }

  // OOBE is completed, so we can safely calculate the device market segment.
  report::MarketSegment device_market_segment =
      GetMarketSegment(g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetDeviceMode(),
                       g_browser_process->platform_part()
                           ->browser_policy_connector_ash()
                           ->GetEnterpriseMarketSegment());

  state_ = State::kReportControllerInitialized;

  // At this step we have checked for 3 conditions.
  // 1. The device is owned.
  // 2. OOBE is completed and .oobe_completed file is written > 1 minute ago.
  // 3. CrosSettingsProvider::TRUSTED: device policies are loaded and trusted.
  report_controller_ = std::make_unique<report::ReportController>(
      ash::report::device_metrics::ChromeDeviceMetadataParameters{
          chrome::GetChannel() /* chromeos_channel */, device_market_segment},
      g_browser_process->local_state(),
      g_browser_process->system_network_context_manager()
          ->GetSharedURLLoaderFactory(),
      std::make_unique<ash::report::device_metrics::PsmClientManager>(
          std::make_unique<
              report::device_metrics::RealPsmClientManagerDelegate>()));
}

}  // namespace ash
