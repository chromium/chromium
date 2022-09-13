// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVER_CONNECTOR_H_
#define CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVER_CONNECTOR_H_

#include "base/callback_forward.h"

namespace apps {

class DeviceInfoManager;

using GetInitialAppsCallback = base::OnceCallback<void()>;

// The AppPreloadServerConnector is used to talk to the App Provisioning Service
// API endpoint. It's role is to build requests and convert responses into
// usable objects.
class AppPreloadServerConnector {
 public:
  AppPreloadServerConnector();
  AppPreloadServerConnector(const AppPreloadServerConnector&) = delete;
  AppPreloadServerConnector& operator=(const AppPreloadServerConnector&) =
      delete;
  ~AppPreloadServerConnector();

  void GetAppsForFirstLogin(const DeviceInfoManager& device_info,
                            GetInitialAppsCallback callback);
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_PRELOAD_SERVICE_APP_PRELOAD_SERVER_CONNECTOR_H_
