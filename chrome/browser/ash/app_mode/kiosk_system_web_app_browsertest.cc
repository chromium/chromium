// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/test_system_web_app_installation.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"

namespace ash {

class KioskSystemWebAppTestDisabled : public WebKioskBaseTest {
 public:
  KioskSystemWebAppTestDisabled() { SetUpSystemWebApps(); }
  KioskSystemWebAppTestDisabled(const KioskSystemWebAppTestDisabled&) = delete;
  KioskSystemWebAppTestDisabled& operator=(
      const KioskSystemWebAppTestDisabled&) = delete;

  Profile* GetProfile() {
    return BrowserList::GetInstance()->get(0)->profile();
  }

 protected:
  void SetUpSystemWebApps() {
    installation_ =
        TestSystemWebAppInstallation::SetUpStandaloneSingleWindowApp();
  }
  std::unique_ptr<TestSystemWebAppInstallation> installation_;
};

class KioskSystemWebAppTestEnabled : public KioskSystemWebAppTestDisabled {
 public:
  KioskSystemWebAppTestEnabled() {
    feature_list_.InitAndEnableFeature(
        ash::features::kKioskEnableSystemWebApps);
    SetUpSystemWebApps();
  }

  KioskSystemWebAppTestEnabled(const KioskSystemWebAppTestEnabled&) = delete;
  KioskSystemWebAppTestEnabled& operator=(const KioskSystemWebAppTestEnabled&) =
      delete;

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(KioskSystemWebAppTestDisabled,
                       ShouldNotLaunchSystemWebApp) {
  InitializeRegularOnlineKiosk();
  EXPECT_EQ(SystemWebAppManager::Get(GetProfile()), nullptr);
}

IN_PROC_BROWSER_TEST_F(KioskSystemWebAppTestEnabled, LaunchSystemWebApp) {
  InitializeRegularOnlineKiosk();
  ASSERT_TRUE(SystemWebAppManager::Get(GetProfile()));
  installation_->WaitForAppInstall();

  content::TestNavigationObserver navigation_observer(
      installation_->GetAppUrl());
  navigation_observer.StartWatchingNewWebContents();
  LaunchSystemWebAppAsync(GetProfile(), installation_->GetType());
  navigation_observer.Wait();
  Browser* swa_browser =
      FindSystemWebAppBrowser(GetProfile(), installation_->GetType());
  EXPECT_NE(swa_browser, nullptr);
}

}  // namespace ash
