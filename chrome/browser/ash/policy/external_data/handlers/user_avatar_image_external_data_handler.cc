// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/user_avatar_image_external_data_handler.h"

#include <utility>

#include "base/check_deref.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_impl.h"
#include "chrome/browser/ash/login/users/avatar/user_image_manager_registry.h"

namespace policy {

namespace {

ash::UserImageManagerImpl* GetUserImageManager(PrefService& local_state,
                                               const std::string& user_id) {
  return ash::UserImageManagerRegistry::Get()->GetManager(
      CloudExternalDataPolicyObserver::GetAccountId(local_state, user_id));
}

}  // namespace

UserAvatarImageExternalDataHandler::UserAvatarImageExternalDataHandler(
    PrefService* local_state)
    : local_state_(CHECK_DEREF(local_state)) {}

UserAvatarImageExternalDataHandler::~UserAvatarImageExternalDataHandler() =
    default;

void UserAvatarImageExternalDataHandler::OnExternalDataSet(
    const std::string& policy,
    const std::string& user_id) {
  GetUserImageManager(local_state_.get(), user_id)->OnExternalDataSet(policy);
}

void UserAvatarImageExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  GetUserImageManager(local_state_.get(), user_id)
      ->OnExternalDataCleared(policy);
}

void UserAvatarImageExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  GetUserImageManager(local_state_.get(), user_id)
      ->OnExternalDataFetched(policy, std::move(data));
}

void UserAvatarImageExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  ash::UserImageManagerRegistry::Get()
      ->GetManager(account_id)
      ->DeleteUserImage();
}

}  // namespace policy
