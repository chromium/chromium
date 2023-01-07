// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_added_removed/user_added_removed_reporter.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/login/users/chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/add_remove_user_event.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"

namespace reporting {

// static
std::unique_ptr<UserAddedRemovedReporter> UserAddedRemovedReporter::Create(
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(
      new UserAddedRemovedReporter(std::make_unique<UserEventReporterHelper>(
                                       Destination::ADDED_REMOVED_EVENTS),
                                   managed_session_service));
}

// static
std::unique_ptr<UserAddedRemovedReporter>
UserAddedRemovedReporter::CreateForTesting(
    std::unique_ptr<UserEventReporterHelper> helper,
    policy::ManagedSessionService* managed_session_service) {
  return base::WrapUnique(
      new UserAddedRemovedReporter(std::move(helper), managed_session_service));
}

UserAddedRemovedReporter::~UserAddedRemovedReporter() = default;

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
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      user->GetType() == user_manager::USER_TYPE_GUEST) {
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
  if (!helper_->ReportingEnabled(ash::kReportDeviceLoginLogout)) {
    return;
  }
  const user_manager::User* user =
      user_manager::UserManager::Get()->FindUser(account_id);
  if (!user || user->IsKioskType() ||
      user->GetType() == user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      user->GetType() == user_manager::USER_TYPE_GUEST) {
    return;
  }

  const std::string email = account_id.GetUserEmail();
  users_to_be_deleted_.insert_or_assign(account_id,
                                        helper_->ShouldReportUser(email));
}

void UserAddedRemovedReporter::OnUserRemoved(
    const AccountId& account_id,
    user_manager::UserRemovalReason reason) {
  if (!helper_->ReportingEnabled(ash::kReportDeviceLoginLogout)) {
    return;
  }
  auto it = users_to_be_deleted_.find(account_id);
  if (it == users_to_be_deleted_.end()) {
    return;
  }

  bool is_affiliated_user = it->second;
  users_to_be_deleted_.erase(it);

  auto record = std::make_unique<UserAddedRemovedRecord>();
  record->mutable_user_removed_event()->set_reason(UserRemovalReason(reason));
  if (is_affiliated_user) {
    record->mutable_affiliated_user()->set_user_email(
        account_id.GetUserEmail());
  }
  record->set_event_timestamp_sec(base::Time::Now().ToTimeT());

  helper_->ReportEvent(std::move(record), ::reporting::Priority::IMMEDIATE);
}

void UserAddedRemovedReporter::ProcessRemoveUserCache() {
  ash::ChromeUserManager* user_manager = ash::ChromeUserManager::Get();
  auto users = user_manager->GetRemovedUserCache();

  for (const auto& user : users) {
    auto record = std::make_unique<UserAddedRemovedRecord>();
    record->set_event_timestamp_sec(base::Time::Now().ToTimeT());
    record->mutable_user_removed_event()->set_reason(
        UserRemovalReason(user.second));
    if (user.first != "") {
      record->mutable_affiliated_user()->set_user_email(user.first);
    }

    helper_->ReportEvent(std::move(record), ::reporting::Priority::IMMEDIATE);
  }

  user_manager->MarkReporterInitialized();
}

UserAddedRemovedReporter::UserAddedRemovedReporter(
    std::unique_ptr<UserEventReporterHelper> helper,
    policy::ManagedSessionService* managed_session_service)
    : helper_(std::move(helper)) {
  ProcessRemoveUserCache();
  managed_session_observation_.Observe(managed_session_service);
}
}  // namespace reporting
