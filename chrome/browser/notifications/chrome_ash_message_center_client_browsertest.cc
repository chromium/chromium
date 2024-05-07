// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include "ash/public/cpp/notifier_settings_controller.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/notifications/notifier_settings_test_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/test/browser_test.h"

namespace {

const char kUrlString[] = "https://example.org";

}  // namespace

class ChromeAshMessageCenterClientBrowserTest : public InProcessBrowserTest {
 public:
  ChromeAshMessageCenterClientBrowserTest() {
    feature_list_.InitWithFeatureState(features::kQuickSettingsPWANotifications,
                                       true);
  }
  ChromeAshMessageCenterClientBrowserTest(
      const ChromeAshMessageCenterClientBrowserTest&) = delete;
  ChromeAshMessageCenterClientBrowserTest& operator=(
      const ChromeAshMessageCenterClientBrowserTest&) = delete;
  ~ChromeAshMessageCenterClientBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_observer_ = std::make_unique<test::NotifierSettingsTestObserver>();
    ASSERT_EQ(0, GetNumberOfNotifiers());
  }

  void TearDownOnMainThread() override {
    test_observer_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void RefreshNotifiers() {
    ash::NotifierSettingsController::Get()->GetNotifiers();
  }

  void SetNotifierEnabled(webapps::AppId app_id, bool enabled) {
    ash::NotifierSettingsController::Get()->SetNotifierEnabled(
        message_center::NotifierId(message_center::NotifierType::APPLICATION,
                                   app_id),
        enabled);
  }

  int GetNumberOfNotifiers() { return test_observer_->notifiers().size(); }

  ash::NotifierMetadata GetNotifierMetadataAtIndex(int index) {
    return test_observer_->notifiers()[index];
  }

  std::string InstallTestPWA() {
    auto web_app_install_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(
            GURL(kUrlString));
    web_app_install_info->display_mode = blink::mojom::DisplayMode::kMinimalUi;
    Profile* profile = browser()->profile();

    // Install a PWA and wait for app service to see it.
    webapps::AppId app_id =
        web_app::test::InstallWebApp(profile, std::move(web_app_install_info));
    // Inform notifier controller it should begin observing |profile|'s'
    // AppRegistryCache.
    RefreshNotifiers();
    return app_id;
  }

 private:
  std::unique_ptr<test::NotifierSettingsTestObserver> test_observer_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeAshMessageCenterClientBrowserTest,
                       PWANotifierControllerTest) {
  std::string app_id = InstallTestPWA();

  // Manually set notification permission to be true and check that notifier
  // list has been updated correctly.
  SetNotifierEnabled(app_id, true);
  RefreshNotifiers();
  ASSERT_EQ(1, GetNumberOfNotifiers());
  ash::NotifierMetadata metadata = GetNotifierMetadataAtIndex(0);
  EXPECT_EQ(app_id, metadata.notifier_id.id);
  EXPECT_EQ(true, metadata.enabled);

  // Manually set notification permission to be false and check that notifier
  // list has been updated correctly.
  SetNotifierEnabled(app_id, false);
  RefreshNotifiers();
  ASSERT_EQ(1, GetNumberOfNotifiers());
  metadata = GetNotifierMetadataAtIndex(0);
  EXPECT_EQ(app_id, metadata.notifier_id.id);
  EXPECT_EQ(false, metadata.enabled);
}
