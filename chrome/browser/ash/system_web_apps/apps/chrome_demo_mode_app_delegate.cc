// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/chrome_demo_mode_app_delegate.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"

namespace ash {

ChromeDemoModeAppDelegate::ChromeDemoModeAppDelegate(content::WebUI* web_ui)
    : web_ui_(web_ui) {}

void ChromeDemoModeAppDelegate::LaunchApp(const std::string& app_id) {
  if (DemoSession::IsDeviceInDemoMode()) {
    DemoSession::RecordAppLaunchSource(
        DemoSession::AppLaunchSource::kDemoModeApp);
  }
  apps::AppServiceProxyFactory::GetForProfile(Profile::FromWebUI(web_ui_))
      ->Launch(app_id, 0, apps::LaunchSource::kFromOtherApp);
}

void ChromeDemoModeAppDelegate::RemoveSplashScreen() {
  DemoSession::Get()->RemoveSplashScreen();
}

}  // namespace ash
