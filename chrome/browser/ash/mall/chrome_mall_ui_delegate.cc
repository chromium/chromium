// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/mall/chrome_mall_ui_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/ash/mall/mall_url.h"
#include "chrome/browser/profiles/profile.h"
#include "url/gurl.h"

namespace ash {

ChromeMallUIDelegate::ChromeMallUIDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui), device_info_manager_(Profile::FromWebUI(web_ui)) {}

ChromeMallUIDelegate::~ChromeMallUIDelegate() = default;

void ChromeMallUIDelegate::GetMallEmbedUrl(
    base::OnceCallback<void(const GURL&)> callback) {
  device_info_manager_.GetDeviceInfo(base::BindOnce([](apps::DeviceInfo info) {
                                       return GetMallLaunchUrl(info);
                                     }).Then(std::move(callback)));
}

}  // namespace ash
