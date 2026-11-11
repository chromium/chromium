// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_added_removed/user_added_removed_reporter.h"

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/add_remove_user_event.pb.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/settings/cros_settings_provider.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace reporting {

// static
std::unique_ptr<UserAddedRemovedReporter> UserAddedRemovedReporter::Create(
    base::flat_map<AccountId, bool> users_to_be_removed,
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(new UserAddedRemovedReporter(
      std::make_unique<UserEventReporterHelper>(
          Destination::ADDED_REMOVED_EVENTS),
      std::move(users_to_be_removed), managed_session_service));
}

// static
std::unique_ptr<UserAddedRemovedReporter>
UserAddedRemovedReporter::CreateForTesting(
    std::unique_ptr<UserEventReporterHelper> helper,
    base::flat_map<AccountId, bool> users_to_be_removed,
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(new UserAddedRemovedReporter(
      std::move(helper), std::move(users_to_be_removed),
      managed_session_service));
}

UserAddedRemovedReporter::~UserAddedRemovedReporter() = default;

// static
bool UserAddedRemovedReporter::IsReportableUserRemoval(
    const AccountId& account_id) {
  // Guest is not reported.
  if (account_id == user_manager::GuestAccountId()) {
    return false;
  }

  // Deprecated accounts, which are removed on start up, are not reported:
  // - Deprecated ARC kiosk accounts. Note that this needs to be checked before
  //   IsDeviceLocalAccountUser(), because the domain is no longer recognized
  //   as a known device local account type.
  // - Legacy supervised users.
  const std::string domain = gaia::ExtractDomainName(account_id.GetUserEmail());
  if (domain == user_manager::kArcKioskDomain ||
      domain == user_manager::kSupervisedUserDomain) {
    return false;
  }

  // Device local accounts (i.e. public sessions and kiosks) are not reported.
  return !policy::IsDeviceLocalAccountUser(account_id.GetUserEmail());
}

void UserAddedRemovedReporter::ProcessRemovedUser(
    std::string_view user_email,
    user_manager::UserRemovalReason reason) {
  ReportRemovedUser(std::string(user_email), reason);
}

void UserAddedRemovedReporter::ReportRemovedUser(
    std::string user_email,
    user_manager::UserRemovalReason reason) {
  // The removal may be processed before the device settings become trusted,
  // e.g. during the start up. Wait for them to check the reporting policy.
  // Note: `user_email` is not moved, because it is used below.
  switch (ash::CrosSettings::Get()->PrepareTrustedValues(
      base::BindOnce(&UserAddedRemovedReporter::ReportRemovedUser,
                     weak_factory_.GetWeakPtr(), user_email, reason))) {
    case ash::CrosSettingsProvider::TRUSTED:
      break;
    case ash::CrosSettingsProvider::TEMPORARILY_UNTRUSTED:
      // Will be called back.
      return;
    case ash::CrosSettingsProvider::PERMANENTLY_UNTRUSTED:
      return;
  }

  if (!helper_->ReportingEnabled(ash::kReportDeviceLoginLogout)) {
    return;
  }

  auto record = std::make_unique<UserAddedRemovedRecord>();
  record->set_event_timestamp_sec(base::Time::Now().ToTimeT());
  record->mutable_user_removed_event()->set_reason(UserRemovalReason(reason));
  if (!user_email.empty()) {
    record->mutable_affiliated_user()->set_user_email(user_email);
  }

  helper_->ReportEvent(std::move(record), ::reporting::Priority::IMMEDIATE);
}

void UserAddedRemovedReporter::OnLogin(Profile* profile) {
  if (!helper_->ReportingEnabled(ash::kReportDeviceLoginLogout)) {
    return;
  }
  if (!helper_->IsCurrentUserNew()) {
    return;
  }
  user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user || user->IsKioskType() ||
      user->GetType() == user_manager::UserType::kPublicAccount ||
      user->GetType() == user_manager::UserType::kGuest) {
    return;
  }

  auto email = user->GetAccountId().GetUserEmail();
  auto record = std::make_unique<UserAddedRemovedRecord>();
  record->mutable_user_added_event();
  if (helper_->ShouldReportUser(email)) {
    record->mutable_affiliated_user()->set_user_email(email);
  }
  record->set_event_timestamp_sec(base::Time::Now().ToTimeT());

  helper_->ReportEvent(std::move(record), ::reporting::Priority::IMMEDIATE);
}

void UserAddedRemovedReporter::OnUserToBeRemoved(const AccountId& account_id) {
  // Note: the reporting policy is checked on reporting. See
  // ReportRemovedUser().
  if (!IsReportableUserRemoval(account_id)) {
    return;
  }

  const std::string email = account_id.GetUserEmail();
  users_to_be_removed_.insert_or_assign(account_id,
                                        helper_->ShouldReportUser(email));
}

void UserAddedRemovedReporter::OnUserRemoved(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  auto it = users_to_be_removed_.find(account_id);
  if (it == users_to_be_removed_.end()) {
    return;
  }

  bool is_affiliated_user = it->second;
  users_to_be_removed_.erase(it);

  ProcessRemovedUser(is_affiliated_user ? account_id.GetUserEmail() : "",
                     reason);
}

UserAddedRemovedReporter::UserAddedRemovedReporter(
    std::unique_ptr<UserEventReporterHelper> helper,
    base::flat_map<AccountId, bool> users_to_be_removed,
    policy::ManagedSessionService* managed_session_service)
    : helper_(std::move(helper)),
      users_to_be_removed_(std::move(users_to_be_removed)) {
  managed_session_observation_.Observe(managed_session_service);
}

}  // namespace reporting
