// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/wallpaper_image_external_data_handler.h"

#include <utility>

#include "chrome/browser/ash/settings/cros_settings.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "components/policy/policy_constants.h"

namespace policy {

WallpaperImageExternalDataHandler::WallpaperImageExternalDataHandler(
    ash::CrosSettings* cros_settings,
    DeviceLocalAccountPolicyService* policy_service)
    : wallpaper_image_observer_(cros_settings,
                                policy_service,
                                key::kWallpaperImage,
                                this) {
  wallpaper_image_observer_.Init();
}

WallpaperImageExternalDataHandler::~WallpaperImageExternalDataHandler() =
    default;

void WallpaperImageExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  WallpaperControllerClientImpl::Get()->RemovePolicyWallpaper(
      CloudExternalDataPolicyHandler::GetAccountId(user_id));
}

void WallpaperImageExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  WallpaperControllerClientImpl::Get()->SetPolicyWallpaper(
      CloudExternalDataPolicyHandler::GetAccountId(user_id), std::move(data));
}

void WallpaperImageExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  WallpaperControllerClientImpl::Get()->RemoveUserWallpaper(account_id);
}

}  // namespace policy
