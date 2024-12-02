// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"

#include <string>
#include <string_view>

#include "apps/test/app_window_waiter.h"
#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/notimplemented.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom-shared.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash::kiosk::test {

KioskApp AutoLaunchKioskApp() {
  auto app = KioskController::Get().GetAutoLaunchApp();
  CHECK(app.has_value());
  return app.value();
}

KioskApp TheKioskApp() {
  auto apps = KioskController::Get().GetApps();
  CHECK_EQ(apps.size(), 1ul);
  return apps[0];
}

KioskApp TheKioskChromeApp() {
  auto app = TheKioskApp();
  CHECK_EQ(app.id().type, KioskAppType::kChromeApp);
  return app;
}

KioskApp TheKioskWebApp() {
  auto app = TheKioskApp();
  CHECK_EQ(app.id().type, KioskAppType::kWebApp);
  return app;
}

bool IsChromeAppInstalled(Profile& profile, const KioskApp& app) {
  CHECK_EQ(app.id().type, KioskAppType::kChromeApp);
  return IsChromeAppInstalled(profile, app.id().app_id.value());
}

bool IsChromeAppInstalled(Profile& profile, std::string_view app_id) {
  auto* chrome_app =
      extensions::ExtensionRegistry::Get(&profile)->GetInstalledExtension(
          std::string(app_id));
  return chrome_app != nullptr;
}

bool IsWebAppInstalled(Profile& profile, const KioskApp& app) {
  CHECK_EQ(app.id().type, KioskAppType::kWebApp);
  return IsWebAppInstalled(profile, app.url().value());
}

bool IsWebAppInstalled(Profile& profile, const GURL& install_url) {
  auto [state, __] = chromeos::GetKioskWebAppInstallState(profile, install_url);
  return crosapi::mojom::WebKioskInstallState::kInstalled == state;
}

bool IsAppInstalled(Profile& profile, const KioskApp& app) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp:
      return IsChromeAppInstalled(profile, app.id().app_id.value());
    case KioskAppType::kWebApp:
      return IsWebAppInstalled(profile, app.url().value());
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/379633748): Support IWA in KioskMixin.
      NOTIMPLEMENTED();
      return false;
  }
}

Profile& CurrentProfile() {
  return CHECK_DEREF(ProfileManager::GetPrimaryUserProfile());
}

void CloseAppWindow(const KioskApp& app) {
  switch (app.id().type) {
    case KioskAppType::kChromeApp: {
      auto& registry =
          CHECK_DEREF(extensions::AppWindowRegistry::Get(&CurrentProfile()));
      auto& chrome_app_window = CHECK_DEREF(
          apps::AppWindowWaiter(&registry, app.id().app_id.value()).Wait());
      chrome_app_window.GetBaseWindow()->Close();
      break;
    }
    case KioskAppType::kWebApp: {
      EXPECT_GE(BrowserList::GetInstance()->size(), 1u);
      auto& web_app_browser = CHECK_DEREF(BrowserList::GetInstance()->get(0));
      web_app_browser.window()->Close();
      break;
    }
    case KioskAppType::kIsolatedWebApp:
      // TODO(crbug.com/379633748): Support IWA in KioskMixin.
      NOTIMPLEMENTED();
      break;
  }
}

}  // namespace ash::kiosk::test
