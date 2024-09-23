// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/external_data/handlers/wallpaper_image_external_data_handler.h"

#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"

namespace policy {

WallpaperImageExternalDataHandler::WallpaperImageExternalDataHandler() =
    default;

WallpaperImageExternalDataHandler::~WallpaperImageExternalDataHandler() =
    default;

void WallpaperImageExternalDataHandler::OnExternalDataCleared(
    const std::string& policy,
    const std::string& user_id) {
  WallpaperControllerClientImpl::Get()->RemovePolicyWallpaper(
      CloudExternalDataPolicyObserver::GetAccountId(user_id));
}

void WallpaperImageExternalDataHandler::OnExternalDataFetched(
    const std::string& policy,
    const std::string& user_id,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  WallpaperControllerClientImpl::Get()->SetPolicyWallpaper(
      CloudExternalDataPolicyObserver::GetAccountId(user_id), std::move(data));
}

void WallpaperImageExternalDataHandler::RemoveForAccountId(
    const AccountId& account_id) {
  WallpaperControllerClientImpl::Get()->RemoveUserWallpaper(account_id,
                                                            base::DoNothing());
}

}  // namespace policy
