// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_server_connector.h"

#include "base/callback.h"

namespace apps {

AppPreloadServerConnector::AppPreloadServerConnector() = default;

AppPreloadServerConnector::~AppPreloadServerConnector() = default;

void AppPreloadServerConnector::GetAppsForFirstLogin(
    const DeviceInfoManager& device_info,
    GetInitialAppsCallback callback) {
  std::move(callback).Run();
}

}  // namespace apps
