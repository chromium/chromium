// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BUILT_IN_CHROMEOS_APPS_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BUILT_IN_CHROMEOS_APPS_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/menu.h"

class Profile;

namespace apps {

class PublisherHost;

struct AppLaunchParams;

// An app publisher (in the App Service sense) of built-in Chrome OS apps.
//
// See components/services/app_service/README.md.
class BuiltInChromeOsApps : public AppPublisher {
 public:
  explicit BuiltInChromeOsApps(AppServiceProxy* proxy);
  BuiltInChromeOsApps(const BuiltInChromeOsApps&) = delete;
  BuiltInChromeOsApps& operator=(const BuiltInChromeOsApps&) = delete;
  ~BuiltInChromeOsApps() override;

 private:
  friend class PublisherHost;

  void Initialize();

  // apps::AppPublisher overrides.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override;
  void LaunchAppWithParams(AppLaunchParams&& params,
                           LaunchCallback callback) override;
  void GetMenuModel(const std::string& app_id,
                    MenuType menu_type,
                    int64_t display_id,
                    base::OnceCallback<void(MenuItems)> callback) override;

  const raw_ptr<Profile> profile_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_BUILT_IN_CHROMEOS_APPS_H_
