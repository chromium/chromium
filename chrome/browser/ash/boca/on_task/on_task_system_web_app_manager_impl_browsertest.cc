// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/on_task/on_task_system_web_app_manager_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/ash/boca/on_task/locked_session_window_tracker_factory.h"
#include "chrome/browser/ash/boca/on_task/on_task_locked_session_window_tracker.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ui/wm/window_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "url/gurl.h"

using ::boca::LockedNavigationOptions;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::Sequence;

namespace ash::boca {
namespace {

constexpr char kTestUrl[] = "https://www.test.com";

// Mock implementation of the `LockedSessionWindowTracker`.
class LockedSessionWindowTrackerMock : public LockedSessionWindowTracker {
 public:
  explicit LockedSessionWindowTrackerMock(Profile* profile)
      : LockedSessionWindowTracker(
            std::make_unique<OnTaskBlocklist>(
                std::make_unique<policy::URLBlocklistManager>(
                    profile->GetPrefs(),
                    policy::policy_prefs::kUrlBlocklist,
                    policy::policy_prefs::kUrlAllowlist)),
            profile) {}
  ~LockedSessionWindowTrackerMock() override = default;

  MOCK_METHOD(void,
              set_can_start_navigation_throttle,
              (bool is_ready),
              (override));
};

class OnTaskSystemWebAppManagerImplBrowserTest : public InProcessBrowserTest {
 protected:
  OnTaskSystemWebAppManagerImplBrowserTest() {
    // Enable Boca and consumer experience for testing purposes. This is used
    // to set up the Boca SWA for OnTask.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kBoca, features::kBocaConsumer,
                              features::kOnDeviceSpeechRecognition},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    ash::SystemWebAppManager::Get(profile())->InstallSystemAppsForTesting();

    // Set up OnTask session for testing purposes. Especially needed to ensure
    // newly created tabs are not deleted.
    GetOnTaskSessionManager()->OnSessionStarted("test_session",
                                                ::boca::UserIdentity());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  OnTaskSessionManager* GetOnTaskSessionManager() {
    ash::BocaManager* const boca_manager =
        ash::BocaManagerFactory::GetInstance()->GetForProfile(profile());
    return boca_manager->GetOnTaskSessionManager();
  }

  Browser* FindBocaSystemWebAppBrowser() {
    return ash::FindSystemWebAppBrowser(profile(), ash::SystemWebAppType::BOCA);
  }

  Profile* profile() { return browser()->profile(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       LaunchSystemWebAppAsync) {
  // Verify no Boca app is launched initially.
  ASSERT_THAT(FindBocaSystemWebAppBrowser(), IsNull());

  // Launch Boca app and verify launch result.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());

  // Also verify the new app window is the active window and is set up for
  // locked mode transition.
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  EXPECT_TRUE(boca_app_browser->IsLockedForOnTask());
  EXPECT_EQ(boca_app_browser->session_id(),
            system_web_app_manager.GetActiveSystemWebAppWindowID());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       LaunchSystemWebAppAsyncWithCustomUrl) {
  // Verify no Boca app is launched initially.
  ASSERT_THAT(FindBocaSystemWebAppBrowser(), IsNull());

  // Launch Boca app and verify launch result.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback(),
                                                 GURL(kTestUrl));
  ASSERT_TRUE(launch_future.Get());

  // Also verify the new app window is the active window and is set up for
  // locked mode transition.
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  EXPECT_TRUE(boca_app_browser->IsLockedForOnTask());
  EXPECT_EQ(boca_app_browser->session_id(),
            system_web_app_manager.GetActiveSystemWebAppWindowID());

  // Verify the homepage is the custom url.
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);
  content::WebContents* web_contents =
      boca_app_browser->tab_strip_model()->GetWebContentsAt(0);
  content::TestNavigationObserver observer(web_contents);
  observer.Wait();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GURL(kTestUrl));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       CloseSystemWebAppWindow) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Close Boca app and verify there is no active app instance.
  system_web_app_manager.CloseSystemWebAppWindow(
      boca_app_browser->session_id());
  content::RunAllTasksUntilIdle();
  EXPECT_THAT(FindBocaSystemWebAppBrowser(), IsNull());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PinSystemWebAppWindow) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Pin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  EXPECT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsVisible());
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PinSystemWebAppWindowOnTablets) {
  ash::TabletModeControllerTestApi().EnterTabletMode();

  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Pin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  EXPECT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(boca_app_browser->window()->IsVisible());
  EXPECT_TRUE(boca_app_browser->window()->IsToolbarVisible());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       UnpinSystemWebAppWindow) {
  // Launch Boca app and pin it for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  ASSERT_TRUE(boca_app_browser->window()->IsVisible());

  // Unpin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/false, boca_app_browser->session_id());
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       UnpinSystemWebAppWindowWhenInFullscreenMode) {
  // Launch Boca app and pin it for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Toggle fullscreen mode but do not pin the window.
  auto* const fullscreen_controller = boca_app_browser->GetFeatures()
                                          .exclusive_access_manager()
                                          ->fullscreen_controller();
  fullscreen_controller->ToggleBrowserFullscreenMode(/*user_initiated=*/false);
  ASSERT_TRUE(fullscreen_controller->IsFullscreenForBrowser());

  // Attempt to unpin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/false, boca_app_browser->session_id());
  EXPECT_FALSE(fullscreen_controller->IsFullscreenForBrowser());
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PinAndPauseSystemWebAppWindow) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Create tab so we can verify we are on boca homepage when paused.
  system_web_app_manager.CreateBackgroundTabWithUrl(
      boca_app_browser->session_id(), GURL(kTestUrl),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);

  // Pin and pause the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  system_web_app_manager.SetPauseStateForSystemWebAppWindow(
      /*paused=*/true, boca_app_browser->session_id());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_TRUE(boca_app_browser->window()->IsVisible());
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_FALSE(ImmersiveModeController::From(boca_app_browser)->IsEnabled());
  EXPECT_EQ(boca_app_browser->tab_strip_model()->active_index(), 0);

  // Verify that tab switch commands are disabled.
  chrome::BrowserCommandController* const command_controller =
      boca_app_browser->command_controller();
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_NEXT_TAB));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_PREVIOUS_TAB));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_0));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_1));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_2));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_3));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_4));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_5));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_6));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_7));

  // Verify that camera and microphone access are disabled.
  EXPECT_TRUE(
      ash::CameraPrivacySwitchController::Get()->IsCameraAccessForceDisabled());
  EXPECT_TRUE(ash::CrasAudioHandler::Get()->IsInputMuted());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       UnpauseSystemWebAppWindow) {
  // Enable camera and disable microphone.
  ash::CameraPrivacySwitchController::Get()->SetForceDisableCameraAccess(false);
  ash::CrasAudioHandler::Get()->SetInputMute(
      true, ash::CrasAudioHandler::InputMuteChangeMethod::kOther);

  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Disable camera and enable microphone.
  ash::CameraPrivacySwitchController::Get()->SetForceDisableCameraAccess(true);
  ash::CrasAudioHandler::Get()->SetInputMute(
      false, ash::CrasAudioHandler::InputMuteChangeMethod::kOther);

  // Pin and pause the Boca app.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  system_web_app_manager.SetPauseStateForSystemWebAppWindow(
      /*paused=*/true, boca_app_browser->session_id());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_FALSE(ImmersiveModeController::From(boca_app_browser)->IsEnabled());

  // Verify that camera and microphone access are disabled.
  ASSERT_TRUE(
      ash::CameraPrivacySwitchController::Get()->IsCameraAccessForceDisabled());
  ASSERT_TRUE(ash::CrasAudioHandler::Get()->IsInputMuted());

  // Unpause the Boca app and verify result.
  system_web_app_manager.SetPauseStateForSystemWebAppWindow(
      /*paused=*/false, boca_app_browser->session_id());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_TRUE(ImmersiveModeController::From(boca_app_browser)->IsEnabled());

  // Verify that tab switch commands are enabled.
  chrome::BrowserCommandController* const command_controller =
      boca_app_browser->command_controller();
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_NEXT_TAB));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_PREVIOUS_TAB));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_0));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_1));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_2));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_3));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_4));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_5));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_6));
  EXPECT_TRUE(command_controller->IsCommandEnabled(IDC_SELECT_TAB_7));

  // Verify that camera and microphone access are restored to the latest state
  // before pause mode.
  EXPECT_TRUE(
      ash::CameraPrivacySwitchController::Get()->IsCameraAccessForceDisabled());
  EXPECT_FALSE(ash::CrasAudioHandler::Get()->IsInputMuted());
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       CreateBackgroundTabWithUrl) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Boca homepage is by default opened.
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Stop the window tracker while adding the new tabs before resuming it.
  LockedSessionWindowTrackerMock window_tracker(profile());
  Sequence s;
  EXPECT_CALL(window_tracker, set_can_start_navigation_throttle(false))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(window_tracker, set_can_start_navigation_throttle(true))
      .Times(1)
      .InSequence(s);

  // Create tab from the url and verify that Boca has the tab.
  system_web_app_manager.SetWindowTrackerForTesting(&window_tracker);
  system_web_app_manager.CreateBackgroundTabWithUrl(
      boca_app_browser->session_id(), GURL(kTestUrl),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  content::WebContents* web_contents =
      boca_app_browser->tab_strip_model()->GetWebContentsAt(1);
  content::TestNavigationObserver observer(web_contents);
  observer.Wait();
  EXPECT_EQ(web_contents->GetLastCommittedURL(), GURL(kTestUrl));

  // Verify that the restriction is applied to the tab.
  OnTaskBlocklist* const blocklist = window_tracker.on_task_blocklist();
  EXPECT_EQ(
      blocklist
          ->parent_tab_to_nav_filters()[sessions::SessionTabHelper::IdForTab(
              web_contents)],
      LockedNavigationOptions::BLOCK_NAVIGATION);
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       RemoveTabsWithTabIds) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Boca homepage is by default opened.
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Stop the window tracker while adding the new tabs before resuming it.
  LockedSessionWindowTrackerMock window_tracker(profile());
  Sequence s;
  EXPECT_CALL(window_tracker, set_can_start_navigation_throttle(false))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(window_tracker, set_can_start_navigation_throttle(true))
      .Times(1)
      .InSequence(s);

  // Create tab from the url and verify that Boca has the tab.
  system_web_app_manager.SetWindowTrackerForTesting(&window_tracker);
  const SessionID tab_id = system_web_app_manager.CreateBackgroundTabWithUrl(
      boca_app_browser->session_id(), GURL(kTestUrl),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  content::WebContents* const web_contents_1 =
      boca_app_browser->tab_strip_model()->GetWebContentsAt(1);
  content::TestNavigationObserver observer(web_contents_1);
  observer.Wait();
  EXPECT_EQ(web_contents_1->GetLastCommittedURL(), GURL(kTestUrl));

  // Stop the window tracker while removing the new tabs before resuming it.
  EXPECT_CALL(window_tracker, set_can_start_navigation_throttle(false))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(window_tracker, set_can_start_navigation_throttle(true))
      .Times(1)
      .InSequence(s);

  // Remove tab with the tab id and verify that Boca no longer has the tab.
  const std::set<SessionID> tab_ids_to_remove = {tab_id};
  system_web_app_manager.RemoveTabsWithTabIds(boca_app_browser->session_id(),
                                              tab_ids_to_remove);
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);
  content::WebContents* const web_contents_2 =
      boca_app_browser->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_NE(web_contents_2->GetLastCommittedURL(), GURL(kTestUrl));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PreparingSystemWebAppWindow) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Create tab so we can verify it gets cleaned up with window prep.
  system_web_app_manager.CreateBackgroundTabWithUrl(
      boca_app_browser->session_id(), GURL(kTestUrl),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);

  // Verify that the tab is cleaned up after window prep.
  system_web_app_manager.PrepareSystemWebAppWindowForOnTask(
      boca_app_browser->session_id(), /*close_bundle_content=*/true);
  EXPECT_TRUE(boca_app_browser->IsLockedForOnTask());
  views::Widget* const widget = views::Widget::GetWidgetForNativeWindow(
      boca_app_browser->window()->GetNativeWindow());
  // TODO (b/382277303): Verify if resize is disabled in locked fullscreen mode.
  EXPECT_TRUE(widget->widget_delegate()->CanResize());
  EXPECT_FALSE(chromeos::wm::CanFloatWindow(
      boca_app_browser->window()->GetNativeWindow()));
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);
  EXPECT_FALSE(boca_app_browser->ShouldRunUnloadListenerBeforeClosing(
      boca_app_browser->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PreparingSystemWebAppWindowAndPreservingContent) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 1);

  // Create tab so we can verify it does not get cleaned up after window prep.
  system_web_app_manager.CreateBackgroundTabWithUrl(
      boca_app_browser->session_id(), GURL(kTestUrl),
      LockedNavigationOptions::BLOCK_NAVIGATION);
  ASSERT_EQ(boca_app_browser->tab_strip_model()->count(), 2);

  // Verify that the tab is not destroyed after window prep.
  system_web_app_manager.PrepareSystemWebAppWindowForOnTask(
      boca_app_browser->session_id(), /*close_bundle_content=*/false);
  EXPECT_TRUE(boca_app_browser->IsLockedForOnTask());
  EXPECT_EQ(boca_app_browser->tab_strip_model()->count(), 2);
  EXPECT_FALSE(boca_app_browser->ShouldRunUnloadListenerBeforeClosing(
      boca_app_browser->tab_strip_model()->GetActiveWebContents()));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       PreparingSWAWindowAndDevToolsCommandsDisabled) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Verify that dev tools commands are disabled.
  system_web_app_manager.PrepareSystemWebAppWindowForOnTask(
      boca_app_browser->session_id(), /*close_bundle_content=*/false);
  chrome::BrowserCommandController* const command_controller =
      boca_app_browser->command_controller();
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_CONSOLE));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_DEVICES));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_INSPECT));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_TOGGLE));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       UnpinSystemWebAppWindowAndDevToolsCommandsDisabled) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  ASSERT_TRUE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));

  // Unpin the Boca app and verify that dev tools commands are disabled.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/false, boca_app_browser->session_id());
  EXPECT_FALSE(platform_util::IsBrowserLockedFullscreen(boca_app_browser));
  chrome::BrowserCommandController* const command_controller =
      boca_app_browser->command_controller();
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_CONSOLE));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_DEVICES));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_INSPECT));
  EXPECT_FALSE(command_controller->IsCommandEnabled(IDC_DEV_TOOLS_TOGGLE));
}

IN_PROC_BROWSER_TEST_F(OnTaskSystemWebAppManagerImplBrowserTest,
                       GetWindowPinState) {
  // Launch Boca app for testing purposes.
  OnTaskSystemWebAppManagerImpl system_web_app_manager(profile());
  base::test::TestFuture<bool> launch_future;
  system_web_app_manager.LaunchSystemWebAppAsync(launch_future.GetCallback());
  ASSERT_TRUE(launch_future.Get());
  Browser* const boca_app_browser = FindBocaSystemWebAppBrowser();
  ASSERT_THAT(boca_app_browser, NotNull());

  // Pin the Boca app and verify result.
  system_web_app_manager.SetPinStateForSystemWebAppWindow(
      /*pinned=*/true, boca_app_browser->session_id());
  EXPECT_TRUE(
      system_web_app_manager.IsWindowPinned(boca_app_browser->session_id()));
}

}  // namespace
}  // namespace ash::boca
