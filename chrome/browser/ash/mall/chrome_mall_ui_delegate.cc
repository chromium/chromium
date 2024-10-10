// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mall/chrome_mall_ui_delegate.h"

#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/ash/mall/mall_url.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace ash {

ChromeMallUIDelegate::ChromeMallUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

ChromeMallUIDelegate::~ChromeMallUIDelegate() = default;

void ChromeMallUIDelegate::GetMallEmbedUrl(
    std::string_view path,
    base::OnceCallback<void(const GURL&)> callback) {
  apps::DeviceInfoManager* manager =
      apps::DeviceInfoManagerFactory::GetForProfile(
          Profile::FromWebUI(web_ui_));
  CHECK(manager);
  manager->GetDeviceInfo(
      base::BindOnce(
          [](const std::string& path, apps::DeviceInfo info) {
            return GetMallLaunchUrl(info, path);
          },
          std::string(path))
          .Then(std::move(callback)));
}

}  // namespace ash
