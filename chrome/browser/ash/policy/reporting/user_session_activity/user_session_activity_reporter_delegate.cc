// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter_delegate.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "components/user_manager/user_type.h"

namespace reporting {

namespace {

int64_t GetUtcTimeMicrosecondsSinceEpoch() {
  return base::Time::Now().InMillisecondsSinceUnixEpoch() *
         base::Time::kMicrosecondsPerMillisecond;
}
}  // namespace

UserSessionActivityReporterDelegate::UserSessionActivityReporterDelegate(
    std::unique_ptr<reporting::UserEventReporterHelper> reporter_helper,
    std::unique_ptr<ash::power::ml::IdleEventNotifier> idle_event_notifier)
    : reporter_helper_(std::move(reporter_helper)),
      idle_event_notifier_(std::move(idle_event_notifier)) {
  CHECK(reporter_helper_);
  CHECK(idle_event_notifier_);
}

UserSessionActivityReporterDelegate::~UserSessionActivityReporterDelegate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ash::power::ml::IdleEventNotifier::ActivityData
UserSessionActivityReporterDelegate::QueryIdleStatus() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return idle_event_notifier_->GetActivityDataAndReset();
}

bool UserSessionActivityReporterDelegate::IsUserActive(
    const ash::power::ml::IdleEventNotifier::ActivityData& activity_data)
    const {
  // Calculate local time of day because that's how
  // `activity_data.last_activity_time_of_day` is calculated.
  const base::TimeDelta local_time_of_day_now =
      base::Time::Now() - base::Time::Now().LocalMidnight();

  const base::TimeDelta time_since_last_activity =
      local_time_of_day_now - activity_data.last_activity_time_of_day;

  return (time_since_last_activity < kActiveIdleStateCollectionFrequency) ||
         activity_data.is_video_playing;
}

void UserSessionActivityReporterDelegate::ReportSessionActivity() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  reporting::UserSessionActivityRecord record;
  record.CopyFrom(session_activity_);

  if (!reporter_helper_->ReportingEnabled(ash::kReportDeviceActivityTimes)) {
    LOG(ERROR) << "ReportDeviceActivityTimes policy not enabled. Skip "
                  "reporting users session activity";
    return;
  }

  reporter_helper_->ReportEvent(
      std::make_unique<UserSessionActivityRecord>(std::move(record)),
      ::reporting::Priority::SLOW_BATCH, base::BindOnce([](Status status) {
        LOG_IF(ERROR, !status.ok())
            << "Failed to enqueue user session activity, status = " << status;
      }));

  // Reset the idle event notifier so that it doesn't return activity data that
  // happened before now. Also clears `session_activity_`.
  Reset();
}

void UserSessionActivityReporterDelegate::SetSessionStartEvent(
    reporting::SessionStartEvent::Reason reason,
    const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user);

  // Reset the idle event notifier so that it doesn't return activity data that
  // happened before now. Also clears `session_activity_`.
  Reset();

  session_activity_.mutable_session_start()->set_reason(reason);

  session_activity_.mutable_session_start()->set_timestamp_micro(
      GetUtcTimeMicrosecondsSinceEpoch());

  SetUser(&session_activity_, user);
}

void UserSessionActivityReporterDelegate::SetSessionEndEvent(
    reporting::SessionEndEvent::Reason reason,
    const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user);

  session_activity_.mutable_session_end()->set_reason(reason);

  session_activity_.mutable_session_end()->set_timestamp_micro(
      GetUtcTimeMicrosecondsSinceEpoch());

  SetUser(&session_activity_, user);
}

void UserSessionActivityReporterDelegate::AddActiveIdleState(
    bool user_is_active,
    const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(user);

  ActiveIdleState state;
  state.set_timestamp_micro(GetUtcTimeMicrosecondsSinceEpoch());

  if (user_is_active) {
    state.set_state(ActiveIdleState_State_ACTIVE);
  } else {
    state.set_state(ActiveIdleState_State_IDLE);
  }

  session_activity_.mutable_active_idle_states()->Add(std::move(state));

  SetUser(&session_activity_, user);
}

void UserSessionActivityReporterDelegate::Reset() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clear previous activity data from the idle event notifier.
  base::IgnoreResult(idle_event_notifier_->GetActivityDataAndReset());

  session_activity_.Clear();
}

void UserSessionActivityReporterDelegate::SetUser(
    UserSessionActivityRecord* record,
    const user_manager::User* user) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(record);
  CHECK(user);

  // Add email for affiliated users, or a device-unique id for unaffiliated
  // users. Don't set any identifier for managed guests.
  if (user->GetType() != user_manager::UserType::kRegular) {
    return;
  }

  const std::string& email = user->GetAccountId().GetUserEmail();

  if (user->IsAffiliated()) {
    record->mutable_affiliated_user()->set_user_email(std::move(email));
  } else {
    const std::string& unique_id =
        reporter_helper_->GetUniqueUserIdForThisDevice(std::move(email));
    record->mutable_unaffiliated_user()->set_user_id(std::move(unique_id));
  }
}

}  // namespace reporting
