// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"

#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "components/account_id/account_id.h"

namespace ash {

UserImageManagerRegistry::UserImageManagerRegistry(
    user_manager::UserManager* user_manager)
    : user_manager_(user_manager) {}

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

}  // namespace ash
