// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/system_web_apps/apps/boca_web_app_info.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/extensions/chrome_test_extension_loader.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_frame_toolbar_view.h"
#include "chrome/browser/ui/views/web_apps/frame_toolbar/web_app_toolbar_button_container.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/test/test_extension_dir.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::NotNull;

namespace {

class MockBocaSessionObserver : public ash::boca::BocaSessionManager::Observer {
 public:
  MOCK_METHOD(void,
              OnSessionStarted,
              (const std::string& session_id,
               const boca::UserIdentity& producer),
              (override));

  MOCK_METHOD(void,
              OnSessionEnded,
              (const std::string& session_id),
              (override));

  MOCK_METHOD(void, OnAppReloaded, (), (override));
};

class BocaAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 protected:
  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();

    // Register mock observer for testing purposes.
    ash::BocaManager* const boca_manager =
        ash::BocaManagerFactory::GetInstance()->GetForProfile(profile());
    boca_manager->GetBocaSessionManager()->AddObserver(session_observer());
  }

  void LaunchAndWait() {
    content::TestNavigationObserver observer(
        (GURL(ash::boca::kChromeBocaAppUntrustedIndexURL)));
    observer.StartWatchingNewWebContents();
    ash::LaunchSystemWebAppAsync(profile(), ash::SystemWebAppType::BOCA);
    observer.Wait();
  }

  void LoadTestExtension() {
    extensions::TestExtensionDir test_extension_dir;
    test_extension_dir.WriteManifest(
        R"({
            "name": "Pop up extension",
            "version": "1.0",
            "manifest_version": 3,
            "action": {
              "default_popup": "popup.html"
            }
          })");
    test_extension_dir.WriteFile(FILE_PATH_LITERAL("popup.html"), "");
    extensions::ChromeTestExtensionLoader(profile()).LoadExtension(
        test_extension_dir.UnpackedPath());
  }

  NiceMock<MockBocaSessionObserver>* session_observer() {
    return &session_observer_;
  }

 private:
  NiceMock<MockBocaSessionObserver> session_observer_;
};

class BocaAppProviderIntegrationTest : public BocaAppIntegrationTest {
 protected:
  BocaAppProviderIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca},
        /*disabled_features=*/{ash::features::kBocaConsumer});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(BocaAppProviderIntegrationTest,
                       ShouldNotNotifyReloadOnLaunch) {
  EXPECT_CALL(*session_observer(), OnAppReloaded).Times(0);
  LaunchAndWait();
}

IN_PROC_BROWSER_TEST_P(BocaAppProviderIntegrationTest,
                       ShouldHideExtensionsContainerInToolbar) {
  LaunchAndWait();
  const Browser* const boca_app_browser =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  ASSERT_THAT(boca_app_browser, NotNull());
  auto* const web_app_frame_toolbar_view =
      BrowserView::GetBrowserViewForBrowser(boca_app_browser)
          ->web_app_frame_toolbar_for_testing();
  ASSERT_TRUE(web_app_frame_toolbar_view->GetVisible());
  WebAppToolbarButtonContainer* const toolbar_button_container =
      web_app_frame_toolbar_view->get_right_container_for_testing();
  EXPECT_THAT(toolbar_button_container->extensions_container(), IsNull());

  // Load extension and verify extensions container remains missing.
  LoadTestExtension();
  EXPECT_THAT(toolbar_button_container->extensions_container(), IsNull());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    BocaAppProviderIntegrationTest);

class BocaAppConsumerIntegrationTest : public BocaAppIntegrationTest {
 protected:
  BocaAppConsumerIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kBocaConsumer},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(BocaAppConsumerIntegrationTest,
                       ShouldNotifyReloadOnLaunch) {
  EXPECT_CALL(*session_observer(), OnAppReloaded).Times(1);
  LaunchAndWait();
}

IN_PROC_BROWSER_TEST_P(BocaAppConsumerIntegrationTest,
                       ShouldShowExtensionsContainerInToolbar) {
  LaunchAndWait();
  const Browser* const boca_app_browser =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  ASSERT_THAT(boca_app_browser, NotNull());
  auto* const web_app_frame_toolbar_view =
      BrowserView::GetBrowserViewForBrowser(boca_app_browser)
          ->web_app_frame_toolbar_for_testing();
  ASSERT_TRUE(web_app_frame_toolbar_view->GetVisible());
  WebAppToolbarButtonContainer* const toolbar_button_container =
      web_app_frame_toolbar_view->get_right_container_for_testing();
  ASSERT_THAT(toolbar_button_container->extensions_container(), NotNull());
  EXPECT_FALSE(toolbar_button_container->extensions_container()->GetVisible());

  // Install extension and verify extensions container is no longer hidden.
  LoadTestExtension();
  EXPECT_TRUE(toolbar_button_container->extensions_container()->GetVisible());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    BocaAppConsumerIntegrationTest);

}  // namespace
