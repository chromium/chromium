// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"

#include "base/notreached.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace ash {

UserImageManagerRegistry::UserImageManagerRegistry(
    user_manager::UserManager* user_manager)
    : user_manager_(user_manager) {
  observation_.Observe(user_manager);
}

UserImageManagerRegistry::~UserImageManagerRegistry() = default;

UserImageManager* UserImageManagerRegistry::GetManager(
    const AccountId& account_id) {
  auto it = map_.find(account_id);
  if (it == map_.end()) {
    it = map_.emplace(account_id, std::make_unique<UserImageManagerImpl>(
                                      account_id, user_manager_.get()))
             .first;
  }
  return it->second.get();
}

void UserImageManagerRegistry::Shutdown() {
  for (auto& [unused, manager] : map_) {
    manager->Shutdown();
  }
}

void UserImageManagerRegistry::OnUserListLoaded() {
  for (const auto* user : user_manager_->GetUsers()) {
    GetManager(user->GetAccountId())->LoadUserImage();
  }
}

void UserImageManagerRegistry::OnDeviceLocalUserListUpdated() {
  for (const auto* user : user_manager_->GetUsers()) {
    if (user->IsDeviceLocalAccount()) {
      GetManager(user->GetAccountId())->LoadUserImage();
    }
  }
}

void UserImageManagerRegistry::OnUserLoggedIn(const user_manager::User& user) {
  auto user_type = user.GetType();
  bool user_is_new = false;
  bool user_is_local = false;
  switch (user_type) {
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      user_is_new = user_manager_->IsCurrentUserNew();
      user_is_local = false;
      break;
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
      // The UserImageManager chooses a random avatar picture when a user logs
      // in for the first time. Tell the UserImageManager that this user is not
      // new to prevent the avatar from getting changed.
      user_is_new = false;
      user_is_local = true;
      break;
    case user_manager::USER_TYPE_GUEST:
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      // Ignore these users.
      return;
    case user_manager::NUM_USER_TYPES:
      NOTREACHED();
      return;
  }

  GetManager(user.GetAccountId())->UserLoggedIn(user_is_new, user_is_local);
}

void UserImageManagerRegistry::OnUserProfileCreated(
    const user_manager::User& user) {
  if (user.HasGaiaAccount()) {
    GetManager(user.GetAccountId())->UserProfileCreated();
  }
}

}  // namespace ash
