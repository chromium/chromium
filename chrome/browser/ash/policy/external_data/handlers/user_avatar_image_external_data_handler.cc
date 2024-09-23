// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/user_avatar_image_external_data_handler.h"

#include <utility>

#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"

namespace policy {

namespace {

ash::UserImageManagerImpl* GetUserImageManager(const std::string& user_id) {
  return ash::UserImageManagerRegistry::Get()->GetManager(
      CloudExternalDataPolicyObserver::GetAccountId(user_id));
}

}  // namespace

UserAvatarImageExternalDataHandler::UserAvatarImageExternalDataHandler() =
    default;

UserAvatarImageExternalDataHandler::~UserAvatarImageExternalDataHandler() =
    default;

void UserAvatarImageExternalDataHandler::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  GetUserImageManager(user_id)->OnExternalDataSet(policy);
}

void UserAvatarImageExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  GetUserImageManager(user_id)->OnExternalDataCleared(policy);
}

void UserAvatarImageExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetUserImageManager(user_id)->OnExternalDataFetched(policy, std::move(data));
}

void UserAvatarImageExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  ash::UserImageManagerRegistry::Get()
      ->GetManager(account_id)
      ->DeleteUserImage();
}

}  // namespace policy
