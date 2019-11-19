// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/status_collector/child_status_collector.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cstdio>
#include <limits>
#include <set>
#include <sstream>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager.h"
#include "chrome/browser/chromeos/policy/status_collector/child_activity_storage.h"
#include "chrome/browser/chromeos/policy/status_collector/device_status_collector.h"
#include "chrome/browser/chromeos/policy/status_collector/status_collector_state.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/util/version_loader.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/enterprise_reporting.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

namespace em = enterprise_management;

using base::Time;
using base::TimeDelta;

// How much time in the past to store active periods for.
constexpr TimeDelta kMaxStoredPastActivityInterval = TimeDelta::FromDays(30);

// How much time in the future to store active periods for.
constexpr TimeDelta kMaxStoredFutureActivityInterval = TimeDelta::FromDays(2);

// How often the child's usage time is stored.
static constexpr base::TimeDelta kUpdateChildActiveTimeInterval =
    base::TimeDelta::FromSeconds(30);

bool ReadAndroidStatus(
    const policy::ChildStatusCollector::AndroidStatusReceiver& receiver) {
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
  instance->GetStatus(receiver);
  return true;
}

}  // namespace

namespace policy {

class ChildStatusCollectorState : public StatusCollectorState {
 public:
  explicit ChildStatusCollectorState(
      const scoped_refptr<base::SequencedTaskRunner> task_runner,
      const StatusCollectorCallback& response)
      : StatusCollectorState(task_runner, response) {}

  bool FetchAndroidStatus(const DeviceStatusCollector::AndroidStatusFetcher&
                              android_status_fetcher) {
    return android_status_fetcher.Run(base::BindRepeating(
        &ChildStatusCollectorState::OnAndroidInfoReceived, this));
  }

 private:
  ~ChildStatusCollectorState() override = default;

  void OnAndroidInfoReceived(const std::string& status,
                             const std::string& droid_guard_info) {
    // TODO(crbug.com/827386): remove after migration.
    em::AndroidStatus* const session_android_status =
        response_params_.session_status->mutable_android_status();
    session_android_status->set_status_payload(status);
    session_android_status->set_droid_guard_info(droid_guard_info);
    // END.

    em::AndroidStatus* const child_android_status =
        response_params_.child_status->mutable_android_status();
    child_android_status->set_status_payload(status);
    child_android_status->set_droid_guard_info(droid_guard_info);
  }
};

ChildStatusCollector::ChildStatusCollector(
    PrefService* pref_service,
    chromeos::system::StatisticsProvider* provider,
    const AndroidStatusFetcher& android_status_fetcher,
    TimeDelta activity_day_start)
    : StatusCollector(provider, chromeos::CrosSettings::Get()),
      pref_service_(pref_service),
      android_status_fetcher_(android_status_fetcher) {
  // protected fields of `StatusCollector`.
  max_stored_past_activity_interval_ = kMaxStoredPastActivityInterval;
  max_stored_future_activity_interval_ = kMaxStoredFutureActivityInterval;

  // Get the task runner of the current thread, so we can queue status responses
  // on this thread.
  CHECK(base::SequencedTaskRunnerHandle::IsSet());
  task_runner_ = base::SequencedTaskRunnerHandle::Get();

  if (android_status_fetcher_.is_null())
    android_status_fetcher_ = base::BindRepeating(&ReadAndroidStatus);

  update_child_usage_timer_.Start(FROM_HERE, kUpdateChildActiveTimeInterval,
                                  this,
                                  &ChildStatusCollector::UpdateChildUsageTime);
  // Watch for changes to the individual policies that control what the status
  // reports contain.
  base::Closure callback = base::BindRepeating(
      &ChildStatusCollector::UpdateReportingSettings, base::Unretained(this));
  version_info_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceVersionInfo, callback);
  boot_mode_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kReportDeviceBootMode, callback);

  // Watch for changes on the device state to calculate the child's active time.
  chromeos::UsageTimeStateNotifier::GetInstance()->AddObserver(this);

  // Fetch the current values of the policies.
  UpdateReportingSettings();

  // Get the OS version.
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindRepeating(&chromeos::version_loader::GetVersion,
                          chromeos::version_loader::VERSION_FULL),
      base::BindRepeating(&ChildStatusCollector::OnOSVersion,
                          weak_factory_.GetWeakPtr()));

  DCHECK(pref_service_->GetInitializationStatus() !=
         PrefService::INITIALIZATION_STATUS_WAITING);
  activity_storage_ = std::make_unique<ChildActivityStorage>(
      pref_service_, prefs::kUserActivityTimes, activity_day_start);
}

ChildStatusCollector::~ChildStatusCollector() {
  chromeos::UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);
}

TimeDelta ChildStatusCollector::GetActiveChildScreenTime() {
  UpdateChildUsageTime();
  return TimeDelta::FromMilliseconds(
      pref_service_->GetInteger(prefs::kChildScreenTimeMilliseconds));
}

void ChildStatusCollector::UpdateReportingSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  if (chromeos::CrosSettingsProvider::TRUSTED !=
      cros_settings_->PrepareTrustedValues(
          base::BindRepeating(&ChildStatusCollector::UpdateReportingSettings,
                              weak_factory_.GetWeakPtr()))) {
    return;
  }

  // Activity times.
  report_activity_times_ = true;

  // Settings related.
  report_version_info_ = true;
  cros_settings_->GetBoolean(chromeos::kReportDeviceVersionInfo,
                             &report_version_info_);

  report_boot_mode_ = true;
  cros_settings_->GetBoolean(chromeos::kReportDeviceBootMode,
                             &report_boot_mode_);
}

void ChildStatusCollector::OnUsageTimeStateChange(
    chromeos::UsageTimeStateNotifier::UsageTimeState state) {
  UpdateChildUsageTime();
  last_state_active_ =
      state == chromeos::UsageTimeStateNotifier::UsageTimeState::ACTIVE;
}

void ChildStatusCollector::UpdateChildUsageTime() {
  if (!report_activity_times_) {
    return;
  }

  Time now = GetCurrentTime();
  Time reset_time = activity_storage_->GetBeginningOfDay(now);
  if (reset_time > now)
    reset_time -= TimeDelta::FromDays(1);
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
    if (active_seconds < base::TimeDelta::FromSeconds(0) ||
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
    em::DeviceStatusReportRequest* status) {
  UpdateChildUsageTime();

  // Signed-in user is reported in child reporting.
  std::vector<ActivityStorage::ActivityPeriod> activity_times =
      activity_storage_->GetStoredActivityPeriods();

  bool anything_reported = false;
  for (const auto& activity_period : activity_times) {
    // This is correct even when there are leap seconds, because when a leap
    // second occurs, two consecutive seconds have the same timestamp.
    int64_t end_timestamp =
        activity_period.start_timestamp + Time::kMillisecondsPerDay;

    em::ActiveTimePeriod* active_period = status->add_active_periods();
    em::TimePeriod* period = active_period->mutable_time_period();
    period->set_start_timestamp(activity_period.start_timestamp);
    period->set_end_timestamp(end_timestamp);
    active_period->set_active_duration(activity_period.activity_milliseconds);
    // Report user email only if users reporting is turned on.
    if (!activity_period.user_email.empty())
      active_period->set_user_email(activity_period.user_email);
    if (activity_period.start_timestamp >= last_reported_day_) {
      last_reported_day_ = activity_period.start_timestamp;
      duration_for_last_reported_day_ = activity_period.activity_milliseconds;
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool ChildStatusCollector::GetActivityTimes(
    em::ChildStatusReportRequest* status) {
  UpdateChildUsageTime();

  // Signed-in user is reported in child reporting.
  std::vector<ActivityStorage::ActivityPeriod> activity_times =
      activity_storage_->GetStoredActivityPeriods();

  bool anything_reported = false;
  for (const auto& activity_period : activity_times) {
    // This is correct even when there are leap seconds, because when a leap
    // second occurs, two consecutive seconds have the same timestamp.
    int64_t end_timestamp =
        activity_period.start_timestamp + Time::kMillisecondsPerDay;

    em::ScreenTimeSpan* screen_time_span = status->add_screen_time_span();
    em::TimePeriod* period = screen_time_span->mutable_time_period();
    period->set_start_timestamp(activity_period.start_timestamp);
    period->set_end_timestamp(end_timestamp);
    screen_time_span->set_active_duration_ms(
        activity_period.activity_milliseconds);
    if (activity_period.start_timestamp >= last_reported_day_) {
      last_reported_day_ = activity_period.start_timestamp;
      duration_for_last_reported_day_ = activity_period.activity_milliseconds;
    }
    anything_reported = true;
  }
  return anything_reported;
}

bool ChildStatusCollector::GetVersionInfo(
    em::DeviceStatusReportRequest* status) {
  status->set_os_version(os_version_);
  return true;
}

bool ChildStatusCollector::GetVersionInfo(
    em::ChildStatusReportRequest* status) {
  status->set_os_version(os_version_);
  return true;
}

void ChildStatusCollector::GetStatusAsync(
    const StatusCollectorCallback& response) {
  // Must be on creation thread since some stats are written to in that thread
  // and accessing them from another thread would lead to race conditions.
  DCHECK(thread_checker_.CalledOnValidThread());

  // Some of the data we're collecting is gathered in background threads.
  // This object keeps track of the state of each async request.
  scoped_refptr<ChildStatusCollectorState> state(
      new ChildStatusCollectorState(task_runner_, response));

  // Gather status data. The following calls might queue some async queries.
  GetDeviceStatus(state);
  GetSessionStatus(state);
  FillChildStatusReportRequest(state);

  // If there are no outstanding async queries, the destructor of |state| calls
  // |response|. If there are async queries, the queries hold references to
  // |state|, so that |state| is only destroyed when the last async query has
  // finished.
}

void ChildStatusCollector::GetDeviceStatus(
    scoped_refptr<ChildStatusCollectorState> state) {
  em::DeviceStatusReportRequest* status =
      state->response_params().device_status.get();
  bool anything_reported = false;

  if (report_version_info_)
    anything_reported |= GetVersionInfo(status);

  if (report_activity_times_)
    anything_reported |= GetActivityTimes(status);

  if (report_boot_mode_) {
    base::Optional<std::string> boot_mode =
        StatusCollector::GetBootMode(statistics_provider_);
    if (boot_mode) {
      status->set_boot_mode(*boot_mode);
      anything_reported = true;
    }
  }

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->response_params().device_status.reset();
}

bool ChildStatusCollector::GetSessionStatusForUser(
    scoped_refptr<ChildStatusCollectorState> state,
    em::SessionStatusReportRequest* status,
    const user_manager::User* user) {
  // Child accounts are not local accounts.
  DCHECK(!user->IsDeviceLocalAccount());

  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return false;

  // Time zone.
  const std::string current_timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());
  status->set_time_zone(current_timezone);

  // Android status.
  const bool report_android_status =
      profile->GetPrefs()->GetBoolean(prefs::kReportArcStatusEnabled);
  if (report_android_status)
    GetAndroidStatus(state);

  status->set_user_dm_token(GetDMTokenForProfile(profile));

  // At least time zone is always reported.
  return true;
}

bool ChildStatusCollector::FillUserSpecificFields(
    scoped_refptr<ChildStatusCollectorState> state,
    em::ChildStatusReportRequest* status,
    const user_manager::User* user) {
  Profile* const profile =
      chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  if (!profile)
    return false;

  // Time zone.
  const std::string current_timezone =
      base::UTF16ToUTF8(chromeos::system::TimezoneSettings::GetInstance()
                            ->GetCurrentTimezoneID());
  status->set_time_zone(current_timezone);

  // Android status.
  const bool report_android_status =
      profile->GetPrefs()->GetBoolean(prefs::kReportArcStatusEnabled);
  if (report_android_status)
    GetAndroidStatus(state);

  if (!user->IsDeviceLocalAccount())
    status->set_user_dm_token(GetDMTokenForProfile(profile));

  // At least time zone is always reported.
  return true;
}

void ChildStatusCollector::GetSessionStatus(
    scoped_refptr<ChildStatusCollectorState> state) {
  em::SessionStatusReportRequest* status =
      state->response_params().session_status.get();
  bool anything_reported = false;

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const primary_user = user_manager->GetPrimaryUser();
  DCHECK(primary_user != nullptr);

  anything_reported |= GetSessionStatusForUser(state, status, primary_user);

  // Wipe pointer if we didn't actually add any data.
  if (!anything_reported)
    state->response_params().session_status.reset();
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

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  const user_manager::User* const primary_user = user_manager->GetPrimaryUser();
  DCHECK(primary_user != nullptr);

  anything_reported |= FillUserSpecificFields(state, status, primary_user);

  if (report_version_info_)
    anything_reported |= GetVersionInfo(status);

  if (report_activity_times_)
    anything_reported |= GetActivityTimes(status);

  if (report_boot_mode_) {
    base::Optional<std::string> boot_mode =
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
  activity_storage_->TrimActivityPeriods(last_reported_day_,
                                         duration_for_last_reported_day_,
                                         std::numeric_limits<int64_t>::max());
}

bool ChildStatusCollector::ShouldReportActivityTimes() const {
  return report_activity_times_;
}

bool ChildStatusCollector::ShouldReportNetworkInterfaces() const {
  return false;
}

bool ChildStatusCollector::ShouldReportUsers() const {
  return false;
}

bool ChildStatusCollector::ShouldReportHardwareStatus() const {
  return false;
}

void ChildStatusCollector::OnOSVersion(const std::string& version) {
  os_version_ = version;
}

}  // namespace policy
