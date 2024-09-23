// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/lock_unlock_reporter.h"

#include <utility>

#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/browser_process.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_names.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace reporting {

namespace {

// TODO(b/247594522): Switch to a single enum.
UnlockType GetUnlockTypeForEvent(
    const session_manager::UnlockType unlock_type) {
  UnlockType converted_unlock_type;
  switch (unlock_type) {
    case session_manager::UnlockType::PASSWORD:
      converted_unlock_type = UnlockType::PASSWORD;
      break;
    case session_manager::UnlockType::PIN:
      converted_unlock_type = UnlockType::PIN;
      break;
    case session_manager::UnlockType::FINGERPRINT:
      converted_unlock_type = UnlockType::FINGERPRINT;
      break;
    case session_manager::UnlockType::CHALLENGE_RESPONSE:
      converted_unlock_type = UnlockType::CHALLENGE_RESPONSE;
      break;
    case session_manager::UnlockType::EASY_UNLOCK:
      converted_unlock_type = UnlockType::EASY_UNLOCK;
      break;
    case session_manager::UnlockType::UNKNOWN:
      converted_unlock_type = UnlockType::UNLOCK_TYPE_UNKNOWN;
      break;
  }
  return converted_unlock_type;
}

}  // namespace

LockUnlockReporter::LockUnlockReporter(
    std::unique_ptr<::reporting::UserEventReporterHelper> helper,
    policy::ManagedSessionService* managed_session_service,
    base::Clock* clock)
    : clock_(clock), helper_(std::move(helper)) {
  if (managed_session_service) {
    managed_session_observation_.Observe(managed_session_service);
  }
}

LockUnlockReporter::~LockUnlockReporter() = default;

// static
std::unique_ptr<LockUnlockReporter> LockUnlockReporter::Create(
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(new LockUnlockReporter(
      std::make_unique<::reporting::UserEventReporterHelper>(
          ::reporting::Destination::LOCK_UNLOCK_EVENTS),
      managed_session_service));
}

// static
std::unique_ptr<LockUnlockReporter> LockUnlockReporter::CreateForTest(
    std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
    policy::ManagedSessionService* managed_session_service,
    base::Clock* clock) {
  return base::WrapUnique(new LockUnlockReporter(
      std::move(reporter_helper), managed_session_service, clock));
}

void LockUnlockReporter::MaybeReportEvent(LockUnlockRecord record) {
  if (!helper_->ReportingEnabled(kReportDeviceLoginLogout)) {
    return;
  }
  const std::string& user_email =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetDisplayEmail();
  if (helper_->ShouldReportUser(user_email)) {
    record.mutable_affiliated_user()->set_user_email(user_email);
  } else {
    // This is an unaffiliated user. We can't report any personal information
    // about them, so we report a device-unique user id instead.
    record.mutable_unaffiliated_user()->set_user_id(
        helper_->GetUniqueUserIdForThisDevice(user_email));
  }
  record.set_event_timestamp_sec(clock_->Now().ToTimeT());

  helper_->ReportEvent(std::make_unique<LockUnlockRecord>(std::move(record)),
                       ::reporting::Priority::SECURITY);
}

void LockUnlockReporter::OnLocked() {
  LockUnlockRecord record;
  record.mutable_lock_event();
  MaybeReportEvent(std::move(record));
}

void LockUnlockReporter::OnUnlockAttempt(
    const bool success,
    const session_manager::UnlockType unlock_type) {
  LockUnlockRecord record;
  UnlockType converted_unlock_type = GetUnlockTypeForEvent(unlock_type);
  record.mutable_unlock_event()->set_unlock_type(converted_unlock_type);
  record.mutable_unlock_event()->set_success(success);
  MaybeReportEvent(std::move(record));
}

}  // namespace reporting
}  // namespace ash
