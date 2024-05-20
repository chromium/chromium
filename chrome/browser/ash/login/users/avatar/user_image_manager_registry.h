// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_REGISTRY_H_
#define CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_REGISTRY_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/user_manager/user_manager.h"

class AccountId;

namespace user_manager {
class UserManager;
}  // namespace user_manager

namespace ash {

class UserImageManagerImpl;
class UserImageLoaderDelegate;

// UserImageManger is per user. This manages the mapping from each user
// identified by AccountId to UserImageManager.
// This is effectively a singleton in production.
class UserImageManagerRegistry : public user_manager::UserManager::Observer {
 public:
  // Returns the global UserImageManagerRegistry instance.
  static UserImageManagerRegistry* Get();

  // Given user_manager's lifetime needs to outlive this instance.
  explicit UserImageManagerRegistry(user_manager::UserManager* user_manager);

  // Constructor to inject a test version of `UserImageLoaderDelegate`.
  UserImageManagerRegistry(
      user_manager::UserManager* user_manager,
      std::unique_ptr<UserImageLoaderDelegate> user_image_loader_delegate);

  UserImageManagerRegistry(const UserImageManagerRegistry&) = delete;
  UserImageManagerRegistry operator=(UserImageManagerRegistry&) = delete;

  ~UserImageManagerRegistry() override;

  // Returns the manager for the given avator.
  // If it is not instantiated, the call lazily creates the instance,
  // and returns the pointer.
  UserImageManagerImpl* GetManager(const AccountId& account_id);

  // Shuts down all UserImageManager this instance holds.
  void Shutdown();

  // user_manager::UserManager::Observer:
  void OnUserListLoaded() override;
  void OnDeviceLocalUserListUpdated() override;
  void OnUserLoggedIn(const user_manager::User& user) override;
  void OnUserProfileCreated(const user_manager::User& user) override;

 private:
  // Owned. Expected to outlive `map_` as it is shared by every
  // `UserImageManagerImpl`.
  const std::unique_ptr<UserImageLoaderDelegate> user_image_loader_delegate_;

  const raw_ptr<user_manager::UserManager> user_manager_;

  std::map<AccountId, std::unique_ptr<UserImageManagerImpl>> map_;

  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_USERS_AVATAR_USER_IMAGE_MANAGER_REGISTRY_H_
