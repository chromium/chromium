// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_WALLPAPER_IMAGE_EXTERNAL_DATA_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_WALLPAPER_IMAGE_EXTERNAL_DATA_HANDLER_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/policy/external_data/handlers/cloud_external_data_policy_handler.h"

namespace ash {
class CrosSettings;
}  // namespace ash

namespace policy {

class DeviceLocalAccountPolicyService;

class WallpaperImageExternalDataHandler
    : public CloudExternalDataPolicyHandler {
 public:
  WallpaperImageExternalDataHandler(
      ash::CrosSettings* cros_settings,
      DeviceLocalAccountPolicyService* policy_service);

  WallpaperImageExternalDataHandler(const WallpaperImageExternalDataHandler&) =
      delete;
  WallpaperImageExternalDataHandler& operator=(
      const WallpaperImageExternalDataHandler&) = delete;

  ~WallpaperImageExternalDataHandler() override;

  // CloudExternalDataPolicyHandler:
  void OnExternalDataCleared(const std::string& policy,
                             const std::string& user_id) override;
  void OnExternalDataFetched(const std::string& policy,
                             const std::string& user_id,
                             std::unique_ptr<std::string> data,
                             const base::FilePath& file_path) override;
  void RemoveForAccountId(const AccountId& account_id) override;

 private:
  CloudExternalDataPolicyObserver wallpaper_image_observer_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_EXTERNAL_DATA_HANDLERS_WALLPAPER_IMAGE_EXTERNAL_DATA_HANDLER_H_
