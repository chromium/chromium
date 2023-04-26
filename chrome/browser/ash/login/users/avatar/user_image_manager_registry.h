// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_REGISTRY_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_REGISTRY_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"

class AccountId;

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

class UserImageManager;

// UserImageManger is per user. This manages the mapping from each user
// identified by AccountId to UserImageManager.
// This is effectively a singleton in production.
class UserImageManagerRegistry {
 public:
  explicit UserImageManagerRegistry(user_manager::UserManager* user_manager);
  UserImageManagerRegistry(const UserImageManagerRegistry&) = delete;
  UserImageManagerRegistry operator=(UserImageManagerRegistry&) = delete;
  ~UserImageManagerRegistry();

  // Returns the manager for the given avator.
  // If it is not instantiated, the call lazily creates the instance,
  // and returns the pointer.
  UserImageManager* GetManager(const AccountId& account_id);

  // Shuts down all UserImageManager this instance holds.
  void Shutdown();

 private:
  const base::raw_ptr<user_manager::UserManager, ExperimentalAsh> user_manager_;
  std::map<AccountId, std::unique_ptr<UserImageManager>> map_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_REGISTRY_H_
