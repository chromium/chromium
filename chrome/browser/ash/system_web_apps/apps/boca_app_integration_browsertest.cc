// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/window_state.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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
#include "chrome/test/base/ui_test_utils.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
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

  MOCK_METHOD(void,
              OnSessionCaptionConfigUpdated,
              (const std::string& group_name,
               const ::boca::CaptionsConfig& config,
               const std::string& tachyon_group_id),
              (override));
};

class BocaAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 protected:
  void SetUpOnMainThread() override {
    SystemWebAppIntegrationTest::SetUpOnMainThread();
    WaitForTestSystemAppInstall();

    // Register mock observer for testing purposes.
    boca_session_manager()->AddObserver(session_observer());
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

  std::unique_ptr<::boca::Session> GetActiveSession() {
    ::boca::SessionConfig session_config;
    session_config.mutable_captions_config()->set_captions_enabled(true);
    std::unique_ptr<::boca::Session> session =
        std::make_unique<::boca::Session>();
    session->mutable_student_group_configs()->insert(
        {ash::boca::kMainStudentGroupName, session_config});
    session->set_session_id("session-id");
    session->set_session_state(::boca::Session::ACTIVE);
    return session;
  }

  ash::boca::BocaSessionManager* boca_session_manager() {
    return ash::BocaManagerFactory::GetInstance()
        ->GetForProfile(profile())
        ->GetBocaSessionManager();
  }

 private:
  NiceMock<MockBocaSessionObserver> session_observer_;
};

class BocaAppProviderIntegrationTest : public BocaAppIntegrationTest {
 protected:
  BocaAppProviderIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kBoca,
                              ash::features::kOnDeviceSpeechRecognition},
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
                       ShouldEndSessionWhenLastAppWindowClose) {
  LaunchAndWait();
  base::test::TestFuture<void> future;
  boca_session_manager()->set_end_session_callback_for_testing(
      future.GetCallback());
  Browser* const boca_app_browser =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  boca_app_browser->window()->Close();
  EXPECT_TRUE(future.Wait());
  EXPECT_FALSE(boca_session_manager()->end_session_callback_for_testing());
}

IN_PROC_BROWSER_TEST_P(BocaAppProviderIntegrationTest,
                       ShouldNotEndSessionWhenStillAppWindowOpen) {
  LaunchAndWait();

  base::test::TestFuture<void> future;
  boca_session_manager()->set_end_session_callback_for_testing(
      future.GetCallback());
  Browser* const boca_app_browser =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);

  // Trigger reload which will cause page handler to be recreated.
  ui_test_utils::NavigateToURLWithDisposition(
      boca_app_browser, GURL(ash::boca::kChromeBocaAppUntrustedIndexURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

  // Callback never executed.
  EXPECT_TRUE(boca_session_manager()->end_session_callback_for_testing());
}

IN_PROC_BROWSER_TEST_P(BocaAppProviderIntegrationTest,
                       ShouldLaunchIntoFloatMode) {
  LaunchAndWait();
  auto* window =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA)
          ->window()
          ->GetNativeWindow();
  ash::WindowState* window_state = ash::WindowState::Get(window);
  EXPECT_TRUE(window_state->IsFloated());
  EXPECT_EQ(400, window->bounds().width());
  EXPECT_EQ(600, window->bounds().height());
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
                              ash::features::kBocaConsumer,
                              ash::features::kOnDeviceSpeechRecognition},
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
                       ShouldNotLaunchIntoFloatMode) {
  LaunchAndWait();
  auto* window =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA)
          ->window()
          ->GetNativeWindow();
  ash::WindowState* window_state = ash::WindowState::Get(window);
  EXPECT_FALSE(window_state->IsFloated());
}

IN_PROC_BROWSER_TEST_P(BocaAppConsumerIntegrationTest,
                       ShouldNotEndSessionWhenAppClose) {
  LaunchAndWait();
  base::test::TestFuture<void> future;
  boca_session_manager()->set_end_session_callback_for_testing(
      future.GetCallback());
  Browser* const boca_app_browser =
      ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  boca_app_browser->window()->Close();
  // Callback never executed.
  EXPECT_TRUE(boca_session_manager()->end_session_callback_for_testing());
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
