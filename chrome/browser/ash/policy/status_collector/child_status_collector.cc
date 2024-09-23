// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/status_collector/child_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <utility>

#include "ash/components/arc/mojom/enterprise_reporting.mojom.h"
#include "ash/components/arc/session/arc_bridge_service.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "base/base64.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/status_collector/child_activity_storage.h"
#include "chrome/browser/ash/policy/status_collector/status_collector_state.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "chromeos/version/version_loader.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// How much time in the past to store active periods for.
constexpr base::TimeDelta kMaxStoredPastActivityInterval = base::Days(30);

// How much time in the future to store active periods for.
constexpr base::TimeDelta kMaxStoredFutureActivityInterval = base::Days(2);

// How often the child's usage time is stored.
constexpr base::TimeDelta kUpdateChildActiveTimeInterval = base::Seconds(30);

const char kReportSizeHistogramName[] =
    "ChromeOS.FamilyLink.ChildStatusReportRequest.Size";
const char kTimeSinceLastReportHistogramName[] =
    "ChromeOS.FamilyLink.ChildStatusReportRequest.TimeSinceLastReport";

bool ReadAndroidStatus(ChildStatusCollector::AndroidStatusReceiver receiver) {
  auto* const arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager)
    return false;
  auto* const instance_holder =
      arc_service_manager->arc_bridge_service()->enterprise_reporting();
  if (!instance_holder)
    return false;
  auto* const instance =
      ARC_GET_INSTANCE_FOR_METHOD(instance_holder, GetStatus);
  if (!instance)
    return false;
  instance->GetStatus(std::move(receiver));
  return true;
}

}  // namespace

class ChildStatusCollectorState : public StatusCollectorState {
 public:
  explicit ChildStatusCollectorState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      StatusCollectorCallback response)
      : StatusCollectorState(task_runner, std::move(response)) {}

  bool FetchAndroidStatus(
      const StatusCollector::AndroidStatusFetcher& android_status_fetcher) {
    return android_status_fetcher.Run(base::BindOnce(
        &ChildStatusCollectorState::OnAndroidInfoReceived, this));
  }

 private:
  ~ChildStatusCollectorState() override = default;

  void OnAndroidInfoReceived(const std::string& status,
                             const std::string& droid_guard_info) {
    em::AndroidStatus* const child_android_status =
        response_params_.child_status->mutable_android_status();
    child_android_status->set_status_payload(status);
    child_android_status->set_droid_guard_info(droid_guard_info);
  }
};

ChildStatusCollector::ChildStatusCollector(
    PrefService* pref_service,
    Profile* profile,
    ash::system::StatisticsProvider* provider,
    const AndroidStatusFetcher& android_status_fetcher,
    base::TimeDelta activity_day_start)
    : StatusCollector(provider, ash::CrosSettings::Get()),
      pref_service_(pref_service),
      profile_(profile),
      android_status_fetcher_(android_status_fetcher) {
  DCHECK(profile_);
  // protected fields of `StatusCollector`.
  max_stored_past_activity_interval_ = kMaxStoredPastActivityInterval;
  max_stored_future_activity_interval_ = kMaxStoredFutureActivityInterval;

  // Get the task runner of the current thread, so we can queue status responses
  // on this thread.
  CHECK(base::SequencedTaskRunner::HasCurrentDefault());
  task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();

  if (android_status_fetcher_.is_null())
    android_status_fetcher_ = base::BindRepeating(&ReadAndroidStatus);

  update_child_usage_timer_.Start(FROM_HERE, kUpdateChildActiveTimeInterval,
                                  this,
                                  &ChildStatusCollector::UpdateChildUsageTime);
  // Watch for changes to the individual policies that control what the status
  // reports contain.
  auto callback = base::BindRepeating(
      &ChildStatusCollector::UpdateReportingSettings, base::Unretained(this));
  version_info_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kReportDeviceVersionInfo, callback);
  boot_mode_subscription_ =
      cros_settings_->AddSettingsObserver(ash::kReportDeviceBootMode, callback);

  // Watch for changes on the device state to calculate the child's active time.
  ash::UsageTimeStateNotifier::GetInstance()->AddObserver(this);

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the OS version.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&chromeos::version_loader::GetVersion,
                     chromeos::version_loader::VERSION_FULL),
      base::BindOnce(&ChildStatusCollector::OnOSVersion,
                     weak_factory_.GetWeakPtr()));

  DCHECK(pref_service_->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING);
  activity_storage_ = std::make_unique<ChildActivityStorage>(
      pref_service_, prefs::kUserActivityTimes, activity_day_start);
}

ChildStatusCollector::~ChildStatusCollector() {
  ash::UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);
}

base::TimeDelta ChildStatusCollector::GetActiveChildScreenTime() {
  UpdateChildUsageTime();
  return base::Milliseconds(
      pref_service_->GetInteger(prefs::kChildScreenTimeMilliseconds));
}

// static
const char* ChildStatusCollector::GetReportSizeHistogramNameForTest() {
  return kReportSizeHistogramName;
}
const char* ChildStatusCollector::GetTimeSinceLastReportHistogramNameForTest() {
  return kTimeSinceLastReportHistogramName;
}

void ChildStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (ash::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&ChildStatusCollector::UpdateReportingSettings,
                         weak_factory_.GetWeakPtr()))) {
    return;
  }

  // Settings related.
  // Keep the default values in sync with DeviceReportingProto in
  // chrome/browser/ash/policy/status_collector/child_status_collector.cc.
  report_version_info_ = true;
  cros_settings_->GetBoolean(ash::kReportDeviceVersionInfo,
                             &report_version_info_);
  report_boot_mode_ = true;
  cros_settings_->GetBoolean(ash::kReportDeviceBootMode, &report_boot_mode_);
}

void ChildStatusCollector::OnAppActivityReportSubmitted() {
  DCHECK(last_report_params_);
  if (last_report_params_->anything_reported) {
    ash::app_time::AppActivityReportInterface* app_activity_reporting =
        ash::app_time::AppActivityReportInterface::Get(profile_);
    DCHECK(app_activity_reporting);
    app_activity_reporting->AppActivityReportSubmitted(
        last_report_params_->generation_time);
  }

  last_report_params_.reset();
}

void ChildStatusCollector::OnUsageTimeStateChange(
    ash::UsageTimeStateNotifier::UsageTimeState state) {
  UpdateChildUsageTime();
  last_state_active_ =
      state == ash::UsageTimeStateNotifier::UsageTimeState::ACTIVE;
}

void ChildStatusCollector::UpdateChildUsageTime() {
  base::Time now = clock_->Now();
  base::Time reset_time = activity_storage_->GetBeginningOfDay(now);
  if (reset_time > now)
    reset_time -= base::Days(1);
  // Reset screen time if it has not been reset today.
  if (reset_time > pref_service_->GetTime(prefs::kLastChildScreenTimeReset)) {
    pref_service_->SetTime(prefs::kLastChildScreenTimeReset, now);
    pref_service_->SetInteger(prefs::kChildScreenTimeMilliseconds, 0);
    pref_service_->CommitPendingWrite();
  }

  if (!last_active_check_.is_null() && last_state_active_) {
    // If it's been too long since the last report, or if the activity is
    // negative (which can happen when the clock changes), assume a single
    // interval of activity. This is the same strategy used to enterprise users.
    base::TimeDelta active_seconds = now - last_active_check_;
    if (active_seconds < base::Seconds(0) ||
        active_seconds >= (2 * kUpdateChildActiveTimeInterval)) {
      activity_storage_->AddActivityPeriod(now - kUpdateChildActiveTimeInterval,
                                           now, now);
    } else {
      activity_storage_->AddActivityPeriod(last_active_check_, now, now);
    }

    activity_storage_->PruneActivityPeriods(
        now, max_stored_past_activity_interval_,
        max_stored_future_activity_interval_);
  }
  last_active_check_ = now;
}

bool ChildStatusCollector::GetActivityTimes(
    em::ChildStatusReportRequest* status) {
  UpdateChildUsageTime();

  // Signed-in user is reported in child reporting.
  auto activity_times = activity_storage_->GetStoredActivityPeriods();

  bool anything_reported = false;
  for (const auto& activity_period : activity_times) {
    // This is correct even when there are leap seconds, because when a leap
    // second occurs, two consecutive seconds have the same timestamp.
    int64_t end_timestamp =
        activity_period.start_timestamp() + base::Time::kMillisecondsPerDay;

    em::ScreenTimeSpan* screen_time_span = status->add_screen_time_span();
    em::TimePeriod* period = screen_time_span->mutable_time_period();
    period->set_start_timestamp(activity_period.start_timestamp());
    period->set_end_timestamp(end_timestamp);
    screen_time_span->set_active_duration_ms(activity_period.end_timestamp() -
                                             activity_period.start_timestamp());
    if (last_reported_end_timestamp_ < end_timestamp) {
      last_reported_end_timestamp_ = end_timestamp;
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool ChildStatusCollector::GetAppActivity(
    em::ChildStatusReportRequest* status) {
  ash::app_time::AppActivityReportInterface* app_activity_reporting =
      ash::app_time::AppActivityReportInterface::Get(profile_);
  DCHECK(app_activity_reporting);

  last_report_params_ =
      app_activity_reporting->GenerateAppActivityReport(status);
  if (last_report_params_->anything_reported) {
    size_t size_in_bytes = status->ByteSizeLong();
    // Logging report size for debugging purposes. Reports larger than 10,485 KB
    // will trigger a hard limit. See CommonJobValidator.java.
    base::UmaHistogramMemoryKB(kReportSizeHistogramName, size_in_bytes / 1024);

    int64_t last_successful_report_time_int = pref_service_->GetInt64(
        prefs::kPerAppTimeLimitsLastSuccessfulReportTime);
    if (last_successful_report_time_int > 0) {
      base::Time last_successful_report_time =
          base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(last_successful_report_time_int));
      DCHECK_LT(last_successful_report_time,
                last_report_params_->generation_time);
      base::TimeDelta elapsed_time =
          last_report_params_->generation_time - last_successful_report_time;
      base::UmaHistogramCounts100000(kTimeSinceLastReportHistogramName,
                                     elapsed_time.InMinutes());
    }
  }
  return last_report_params_->anything_reported;
}

bool ChildStatusCollector::GetVersionInfo(
    em::ChildStatusReportRequest* status) {
  status->set_os_version(os_version_);
  return true;
}

void ChildStatusCollector::GetStatusAsync(StatusCollectorCallback response) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());

  // Some of the data we're collecting is gathered in background threads.
  // This object keeps track of the state of each async request.
  scoped_refptr<ChildStatusCollectorState> state(
      new ChildStatusCollectorState(task_runner_, std::move(response)));

  // Gather status data might queue some async queries.
  FillChildStatusReportRequest(state);

  // If there are no outstanding async queries, the destructor of |state| calls
  // |response|. If there are async queries, the queries hold references to
  // |state|, so that |state| is only destroyed when the last async query has
  // finished.
}

bool ChildStatusCollector::FillUserSpecificFields(
    scoped_refptr<ChildStatusCollectorState> state,
    em::ChildStatusReportRequest* status) {
  // Time zone.
  const std::string current_timezone = base::UTF16ToUTF8(
      ash::system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID());
  status->set_time_zone(current_timezone);

  // Android status.
  const bool report_android_status =
      profile_->GetPrefs()->GetBoolean(prefs::kReportArcStatusEnabled);
  if (report_android_status)
    GetAndroidStatus(state);

  status->set_user_dm_token(GetDMTokenForProfile(profile_));

  // At least time zone is always reported.
  return true;
}

bool ChildStatusCollector::GetAndroidStatus(
    const scoped_refptr<ChildStatusCollectorState>& state) {
  return state->FetchAndroidStatus(android_status_fetcher_);
}

void ChildStatusCollector::FillChildStatusReportRequest(
    scoped_refptr<ChildStatusCollectorState> state) {
  em::ChildStatusReportRequest* status =
      state->response_params().child_status.get();
  bool anything_reported = false;

  anything_reported |= FillUserSpecificFields(state, status);

  if (report_version_info_)
    anything_reported |= GetVersionInfo(status);

  anything_reported |= GetActivityTimes(status);
  anything_reported |= GetAppActivity(status);

  if (report_boot_mode_) {
    std::optional<std::string> boot_mode =
        StatusCollector::GetBootMode(statistics_provider_);
    if (boot_mode) {
      status->set_boot_mode(*boot_mode);
      anything_reported = true;
    }
  }

  // Wipe value if we didn't actually add any data.
  if (!anything_reported)
    state->response_params().child_status.reset();
}

void ChildStatusCollector::OnSubmittedSuccessfully() {
  activity_storage_->TrimActivityPeriods(last_reported_end_timestamp_,
                                         std::numeric_limits<int64_t>::max());
  OnAppActivityReportSubmitted();
}

bool ChildStatusCollector::IsReportingActivityTimes() const {
  return true;
}

bool ChildStatusCollector::IsReportingHardwareData() const {
  return false;
}

bool ChildStatusCollector::IsReportingNetworkData() const {
  return false;
}

bool ChildStatusCollector::IsReportingUsers() const {
  return false;
}

bool ChildStatusCollector::IsReportingCrashReportInfo() const {
  return false;
}
bool ChildStatusCollector::IsReportingAppInfoAndActivity() const {
  return false;
}

// TODO(crbug.com/40239081)
// Make this function fallible when the optional passed in evaluated to
// nullptr, instead of returning a dummy string.
void ChildStatusCollector::OnOSVersion(
    const std::optional<std::string>& version) {
  os_version_ = version.value_or("0.0.0.0");
}

}  // namespace policy
