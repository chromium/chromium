// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_app_launch_handler.h"

#include <cstdint>
#include <map>
#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/autotest_desks_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/split_view_test_api.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_test_helper.h"
#include "chrome/browser/ash/app_restore/app_restore_test_util.h"
#include "chrome/browser/ash/app_restore/arc_app_queue_restore_handler.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/app_restore/full_restore_service.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ash/web_applications/os_url_handler_system_web_app_info.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/desks/desks_client.h"
#include "chrome/browser/ui/ash/device_scheduled_reboot/reboot_notification_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/shell_surface_builder.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/wm/core/window_util.h"

namespace ash::full_restore {

namespace {

constexpr char kAppId[] = "mldnpnnoiloahfhddhobgjeophloidmo";
constexpr int32_t kWindowId1 = 100;
constexpr int32_t kWindowId2 = 200;

constexpr char kTestAppPackage[] = "test.arc.app.package";

// Name of the preference that the SessionID of the Browser is saved to. This
// is used to transfer the SessionID from PRE_ test to actual test.
constexpr char kRestoreIdPrefName[] = "browser_restore_id";

// Test values for a test WindowInfo object.
constexpr int kActivationIndex = 2;
constexpr int kDeskId = 2;
const base::Uuid kDeskUuid = base::Uuid::GenerateRandomV4();
constexpr gfx::Rect kCurrentBounds(500, 200);
constexpr chromeos::WindowStateType kWindowStateType =
    chromeos::WindowStateType::kPrimarySnapped;

void RemoveInactiveDesks() {
  // Removes all the inactive desks and waits for their async operations to
  // complete.
  while (true) {
    base::RunLoop run_loop;
    if (!AutotestDesksApi().RemoveActiveDesk(run_loop.QuitClosure()))
      break;
    run_loop.Run();
  }
}

void ActivateDesk(int index) {
  base::RunLoop run_loop;
  AutotestDesksApi().ActivateDeskAtIndex(index, run_loop.QuitClosure());
  run_loop.Run();
}

// Gets the ARC app launch information from the full restore file for `app_id`
// and `session_id`.
std::unique_ptr<::app_restore::AppLaunchInfo> GetArcAppLaunchInfo(
    const std::string& app_id,
    int32_t session_id) {
  return ::full_restore::FullRestoreReadHandler::GetInstance()
      ->GetArcAppLaunchInfo(app_id, session_id);
}

class TestAppRestoreInfoObserver
    : public ::app_restore::AppRestoreInfo::Observer {
 public:
  // ::app_restore::AppRestoreInfo::Observer:
  void OnAppLaunched(aura::Window* window) override {
    ++launched_windows_[window];
  }

  void OnWindowInitialized(aura::Window* window) override {
    ++initialized_windows_[window];
  }

  void Reset() {
    launched_windows_.clear();
    initialized_windows_.clear();
  }

  std::map<aura::Window*, int>& launched_windows() { return launched_windows_; }
  std::map<aura::Window*, int>& initialized_windows() {
    return initialized_windows_;
  }

 private:
  std::map<aura::Window*, int> launched_windows_;
  std::map<aura::Window*, int> initialized_windows_;
};

// Creates a WindowInfo object and then saves it.
void CreateAndSaveWindowInfo(int desk_id,
                             const base::Uuid& desk_uuid,
                             const gfx::Rect& current_bounds,
                             chromeos::WindowStateType window_state_type,
                             ui::WindowShowState pre_minimized_show_state,
                             int32_t window_id,
                             uint32_t snap_percentage) {
  // A window is needed for SaveWindowInfo, but all it needs is a layer and
  // kWindowIdKey to be set. `window` needs to be alive when save is called for
  // SaveWindowInfo to work.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetProperty(::app_restore::kWindowIdKey, window_id);

  ::app_restore::WindowInfo window_info;
  window_info.window = window.get();
  window_info.desk_id = desk_id;
  window_info.desk_guid = desk_uuid;
  window_info.current_bounds = current_bounds;
  window_info.window_state_type = window_state_type;

  if (pre_minimized_show_state != ui::SHOW_STATE_DEFAULT) {
    DCHECK_EQ(chromeos::WindowStateType::kMinimized, window_state_type);
    window_info.pre_minimized_show_state_type = pre_minimized_show_state;
  }

  if (window_state_type == chromeos::WindowStateType::kPrimarySnapped ||
      window_state_type == chromeos::WindowStateType::kSecondarySnapped) {
    DCHECK_GT(snap_percentage, 0u);
    window_info.snap_percentage = snap_percentage;
  }

  ::full_restore::SaveWindowInfo(window_info);
}

void SaveWindowInfo(aura::Window* window) {
  ::app_restore::WindowInfo window_info;
  window_info.window = window;
  window_info.activation_index = kActivationIndex;
  window_info.desk_id = kDeskId;
  window_info.desk_guid = kDeskUuid;
  window_info.current_bounds = kCurrentBounds;
  window_info.window_state_type = WindowState::Get(window)->GetStateType();
  ::full_restore::SaveWindowInfo(window_info);
}

void SaveWindowInfo(
    aura::Window* window,
    uint32_t activation_index,
    chromeos::WindowStateType window_state_type = kWindowStateType) {
  ::app_restore::WindowInfo window_info;
  window_info.window = window;
  window_info.activation_index = activation_index;
  window_info.desk_id = kDeskId;
  window_info.desk_guid = kDeskUuid;
  window_info.current_bounds = kCurrentBounds;
  window_info.window_state_type = window_state_type;
  ::full_restore::SaveWindowInfo(window_info);
}

// Gets the browser whose restore window id is same as `window_id`.
Browser* GetBrowserForWindowId(int32_t window_id) {
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->window()->GetNativeWindow()->GetProperty(
            ::app_restore::kRestoreWindowIdKey) == window_id) {
      return browser;
    }
  }
  return nullptr;
}

void ClickButton(const views::Button* button) {
  ASSERT_TRUE(button);
  ASSERT_TRUE(button->GetVisible());
  aura::Window* root_window =
      button->GetWidget()->GetNativeWindow()->GetRootWindow();
  ui::test::EventGenerator event_generator(root_window);
  event_generator.MoveMouseToInHost(button->GetBoundsInScreen().CenterPoint());
  event_generator.ClickLeftButton();
}

void ClickSaveDeskAsTemplateButton() {
  ClickButton(GetSaveDeskAsTemplateButton());
  // Wait for the template to be stored in the model.
  WaitForSavedDeskUI();
  // Clicking the save template button selects the newly created template's name
  // field. We can press enter or escape or click to select out of it.
  SendKey(ui::VKEY_RETURN);
}

void ClickTemplateItem(int index) {
  ClickButton(GetSavedDeskItemButton(/*index=*/0));
}

}  // namespace

class FullRestoreAppLaunchHandlerBrowserTest
    : public extensions::PlatformAppBrowserTest {
 public:
  FullRestoreAppLaunchHandlerBrowserTest()
      : faster_animations_(
            ui::ScopedAnimationDurationScaleMode::ZERO_DURATION) {
    scoped_restore_for_testing_ = std::make_unique<ScopedRestoreForTesting>();
    set_launch_browser_for_testing(nullptr);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDesksTemplates},
        /*disabled_features=*/{features::kDeskTemplateSync});
  }
  ~FullRestoreAppLaunchHandlerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    extensions::PlatformAppBrowserTest::SetUpOnMainThread();
    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
  }

  void SetShouldRestore(FullRestoreAppLaunchHandler* app_launch_handler) {
    content::RunAllTasksUntilIdle();
    app_launch_handler->SetShouldRestore();
    content::RunAllTasksUntilIdle();
  }

  void CreateWebApp() {
    auto web_app_install_info = std::make_unique<WebAppInstallInfo>();
    web_app_install_info->start_url = GURL("https://example.org");
    web_app::AppId app_id = web_app::test::InstallWebApp(
        profile(), std::move(web_app_install_info));
  }

  aura::Window* FindWebAppWindow() {
    for (auto* browser : *BrowserList::GetInstance()) {
      aura::Window* window = browser->window()->GetNativeWindow();
      if (window->GetProperty(::app_restore::kRestoreWindowIdKey) ==
          kWindowId2) {
        return window;
      }
    }
    return nullptr;
  }

  void WaitForAppLaunchInfoSaved(bool allow_save = true) {
    ::full_restore::FullRestoreSaveHandler* save_handler =
        ::full_restore::FullRestoreSaveHandler::GetInstance();
    if (allow_save)
      save_handler->AllowSave();
    base::OneShotTimer* timer = save_handler->GetTimerForTesting();
    if (timer->IsRunning()) {
      // Simulate timeout, and the launch info is saved.
      timer->FireNow();
    }
    content::RunAllTasksUntilIdle();

    ::full_restore::FullRestoreReadHandler::GetInstance()
        ->profile_path_to_restore_data_.clear();
  }

  void SaveChromeAppLaunchInfo(const std::string& app_id) {
    ::full_restore::SaveAppLaunchInfo(
        profile()->GetPath(),
        std::make_unique<::app_restore::AppLaunchInfo>(
            app_id, apps::LaunchContainer::kLaunchContainerWindow,
            WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
            std::vector<base::FilePath>{}, nullptr));
  }

  std::unique_ptr<::app_restore::WindowInfo> GetWindowInfo(
      int32_t restore_window_id) {
    return ::full_restore::FullRestoreReadHandler::GetInstance()->GetWindowInfo(
        restore_window_id);
  }

  std::unique_ptr<::app_restore::WindowInfo> GetWindowInfo(
      aura::Window* window) {
    return ::full_restore::FullRestoreReadHandler::GetInstance()->GetWindowInfo(
        window);
  }

  bool HasNotificationFor(const std::string& notification_id) {
    absl::optional<message_center::Notification> message_center_notification =
        display_service()->GetNotification(notification_id);
    return message_center_notification.has_value();
  }

  void VerifyPostRebootNotificationTitle(const std::string& notification_id) {
    absl::optional<message_center::Notification> message_center_notification =
        display_service()->GetNotification(notification_id);
    ASSERT_TRUE(message_center_notification.has_value());
    EXPECT_EQ(message_center_notification.value().title(),
              l10n_util::GetStringUTF16(IDS_POLICY_DEVICE_POST_REBOOT_TITLE));
  }

  void SimulateClick(const std::string& notification_id,
                     RestoreNotificationButtonIndex action_index) {
    FullRestoreService::GetForProfile(profile())->Click(
        static_cast<int>(action_index), absl::nullopt);
  }

  NotificationDisplayServiceTester* display_service() const {
    return display_service_.get();
  }

  void ResetRestoreForTesting() { scoped_restore_for_testing_.reset(); }

 private:
  ui::ScopedAnimationDurationScaleMode faster_animations_;
  std::unique_ptr<ScopedRestoreForTesting> scoped_restore_for_testing_;
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       NoBrowserOnLaunch) {
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       NotLaunchBrowser) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create FullRestoreAppLaunchHandler, and set should restore.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreAndAddApp) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler, and set should restore.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  aura::Window* web_app_window = FindWebAppWindow();
  ASSERT_TRUE(web_app_window);
  EXPECT_TRUE(web_app_window->GetProperty(::app_restore::kWindowInfoKey));
}

// Tests that restoring windows that are minimized will restore their
// pre-minimized window state when unminimizing.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       PreMinimizedState) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));
  CreateAndSaveWindowInfo(
      kDeskId, kDeskUuid, kCurrentBounds, chromeos::WindowStateType::kMinimized,
      ui::SHOW_STATE_MAXIMIZED, kWindowId2, /*snap_percentage=*/0);

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler, and set should restore.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // The web app window should be attainable.
  CreateWebApp();
  content::RunAllTasksUntilIdle();
  aura::Window* app_window = FindWebAppWindow();
  ASSERT_TRUE(app_window);

  // The current window state should be minimized, and when we unminimize it
  // should be maximized.
  WindowState* window_state = WindowState::Get(app_window);
  ASSERT_TRUE(window_state->IsMinimized());
  window_state->Unminimize();
  EXPECT_TRUE(window_state->IsMaximized());

  window_state->Restore();
  EXPECT_EQ(kCurrentBounds, app_window->GetBoundsInScreen());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       AddAppAndRestore) {
  // Add app launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  CreateWebApp();

  SetShouldRestore(app_launch_handler.get());

  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       FirstRunFullRestore) {
  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/true);

  content::RunAllTasksUntilIdle();

  // Verify there is a new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest, NotRestore) {
  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
  EXPECT_FALSE(FindWebAppWindow());
}

// Verify simple post reboot notification is shown when
// |kShowPostRebootNotification| pref is set and restore notification is not
// shown.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       NotRestoreAndShowSimplePostRebootNotification) {
  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  size_t count = BrowserList::GetInstance()->size();

  // Set the pref for showing post reboot notification.
  profile()->GetPrefs()->SetBoolean(prefs::kShowPostRebootNotification, true);

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);

  CreateWebApp();

  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
  EXPECT_FALSE(FindWebAppWindow());
  EXPECT_TRUE(HasNotificationFor(kPostRebootNotificationId));
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreAndLaunchBrowser) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  auto app_launch_info = std::make_unique<::app_restore::AppLaunchInfo>(
      app_constants::kChromeAppId, kWindowId2);
  app_launch_info->app_type_browser = true;
  ::full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(app_launch_info));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

// Verify the restore data is saved when the restore setting is always and the
// restore finishes.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreAndLaunchBrowserWithAlwaysSetting) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  auto app_launch_info = std::make_unique<::app_restore::AppLaunchInfo>(
      app_constants::kChromeAppId, kWindowId2);
  app_launch_info->app_type_browser = true;
  ::full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(app_launch_info));

  WaitForAppLaunchInfoSaved();
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Set the restore pref setting as 'Always restore'.
  profile()->GetPrefs()->SetInteger(kRestoreAppsAndPagesPrefName,
                                    static_cast<int>(RestoreOption::kAlways));

  // Create FullRestoreAppLaunchHandler to simulate the system startup.
  auto* full_restore_service = FullRestoreService::GetForProfile(profile());
  full_restore_service->SetAppLaunchHandlerForTesting(
      std::make_unique<FullRestoreAppLaunchHandler>(
          profile(), /*should_init_service=*/true));
  auto* app_launch_handler1 = full_restore_service->app_launch_handler();

  app_launch_handler1->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());

  WaitForAppLaunchInfoSaved(/*allow_save*/ false);
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Create FullRestoreAppLaunchHandler to simulate the system startup again.
  full_restore_service->SetAppLaunchHandlerForTesting(
      std::make_unique<FullRestoreAppLaunchHandler>(
          profile(), /*should_init_service=*/true));
  auto* app_launch_handler2 = full_restore_service->app_launch_handler();

  app_launch_handler2->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  // Verify there is a new browser launched again.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
}

// Verify the restore data is saved when the restore button is clicked and the
// restore finishes.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreAndLaunchBrowserWithClickRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  auto app_launch_info = std::make_unique<::app_restore::AppLaunchInfo>(
      app_constants::kChromeAppId, kWindowId2);
  app_launch_info->app_type_browser = true;
  ::full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(app_launch_info));

  WaitForAppLaunchInfoSaved();
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Set the restore pref setting as 'Ask every time'.
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));

  // Create FullRestoreAppLaunchHandler to simulate the system startup.
  auto* full_restore_service = FullRestoreService::GetForProfile(profile());
  full_restore_service->SetAppLaunchHandlerForTesting(
      std::make_unique<FullRestoreAppLaunchHandler>(
          profile(), /*should_init_service=*/true));
  auto* app_launch_handler1 = full_restore_service->app_launch_handler();

  app_launch_handler1->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(HasNotificationFor(kRestoreNotificationId));
  SimulateClick(kRestoreForCrashNotificationId,
                RestoreNotificationButtonIndex::kRestore);
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());

  WaitForAppLaunchInfoSaved(/*allow_save*/ false);
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Create FullRestoreAppLaunchHandler to simulate the system startup again.
  auto app_launch_handler2 =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  app_launch_handler2->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();
  SetShouldRestore(app_launch_handler2.get());
  content::RunAllTasksUntilIdle();

  content::RunAllTasksUntilIdle();

  // Verify there is a new browser launched again.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
}

// Verify the restore notification is shown with post reboot notification title
// when |kShowPostRebootNotification| pref is set.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreWithPostRebootTitle) {
  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  auto app_launch_info = std::make_unique<::app_restore::AppLaunchInfo>(
      app_constants::kChromeAppId, kWindowId2);
  app_launch_info->app_type_browser = true;
  ::full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(app_launch_info));

  WaitForAppLaunchInfoSaved();
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Set the restore pref setting as 'Ask every time'.
  profile()->GetPrefs()->SetInteger(
      kRestoreAppsAndPagesPrefName,
      static_cast<int>(RestoreOption::kAskEveryTime));
  // Set the pref for showing post reboot notification.
  profile()->GetPrefs()->SetBoolean(prefs::kShowPostRebootNotification, true);

  // Create FullRestoreAppLaunchHandler to simulate the system startup.
  auto* full_restore_service = FullRestoreService::GetForProfile(profile());
  full_restore_service->SetAppLaunchHandlerForTesting(
      std::make_unique<FullRestoreAppLaunchHandler>(
          profile(), /*should_init_service=*/true));
  auto* app_launch_handler1 = full_restore_service->app_launch_handler();

  app_launch_handler1->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(HasNotificationFor(kRestoreNotificationId));
  VerifyPostRebootNotificationTitle(kRestoreNotificationId);
  EXPECT_FALSE(HasNotificationFor(kPostRebootNotificationId));
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreAndNoBrowserLaunchInfo) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch info, but no browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  // Remove the browser app to mock no browser launch info.
  ::full_restore::FullRestoreSaveHandler::GetInstance()->RemoveApp(
      profile()->GetPath(), app_constants::kChromeAppId);

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       LaunchBrowserAndRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  auto app_launch_info = std::make_unique<::app_restore::AppLaunchInfo>(
      app_constants::kChromeAppId, kWindowId2);
  app_launch_info->app_type_browser = true;
  ::full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(app_launch_info));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  // Verify there is no new browser launched.
  EXPECT_EQ(count, BrowserList::GetInstance()->size());

  // Set should restore.
  app_launch_handler->SetShouldRestore();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 1, BrowserList::GetInstance()->size());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreAndLaunchBrowserAndAddApp) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler, and set should restore.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);

  CreateWebApp();
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
  EXPECT_TRUE(FindWebAppWindow());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       LaunchBrowserAndAddAppAndRestore) {
  size_t count = BrowserList::GetInstance()->size();

  // Add app launch infos.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          kAppId, kWindowId2, apps::LaunchContainer::kLaunchContainerWindow,
          WindowOpenDisposition::NEW_WINDOW, display::kDefaultDisplayId,
          std::vector<base::FilePath>{}, nullptr));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);

  CreateWebApp();

  SetShouldRestore(app_launch_handler.get());

  // Verify there is new browser launched.
  EXPECT_EQ(count + 2, BrowserList::GetInstance()->size());
  EXPECT_TRUE(FindWebAppWindow());
}

// Tests that the window properties on the browser window match the ones we set
// in the window info.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       WindowProperties) {
  size_t count = BrowserList::GetInstance()->size();

  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  constexpr uint32_t kSnapPercentage = 75;
  CreateAndSaveWindowInfo(kDeskId, kDeskUuid, kCurrentBounds, kWindowStateType,
                          ui::SHOW_STATE_DEFAULT, kWindowId1, kSnapPercentage);
  WaitForAppLaunchInfoSaved();

  // Launch the browser.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  SetShouldRestore(app_launch_handler.get());

  ASSERT_EQ(count + 1u, BrowserList::GetInstance()->size());

  // TODO(sammiequon): Check the values from the actual browser window.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  window->SetProperty(::app_restore::kRestoreWindowIdKey, kWindowId1);
  auto stored_window_info = GetWindowInfo(window.get());
  EXPECT_EQ(kDeskId, *stored_window_info->desk_id);
  EXPECT_EQ(kDeskUuid, stored_window_info->desk_guid);
  EXPECT_EQ(kCurrentBounds, *stored_window_info->current_bounds);
  EXPECT_EQ(kWindowStateType, *stored_window_info->window_state_type);
  EXPECT_EQ(kSnapPercentage, *stored_window_info->snap_percentage);
}

// The PRE phase of the FullRestoreOverridesSessionRestoreTest. Creates a
// browser and turns on session restore.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       PRE_FullRestoreOverridesSessionRestoreTest) {
  // Create a browser and create a tab for it. Its bounds should not equal
  // |kCurrentBounds|.
  Browser* browser = Browser::Create(Browser::CreateParams(profile(), true));
  PrefService* local_state = g_browser_process->local_state();
  static_cast<PrefRegistrySimple*>(local_state->DeprecatedGetPrefRegistry())
      ->RegisterIntegerPref(kRestoreIdPrefName, 0);
  local_state->SetInteger(kRestoreIdPrefName, browser->session_id().id());
  AddBlankTabAndShow(browser);
  aura::Window* window = browser->window()->GetNativeWindow();
  ASSERT_NE(kCurrentBounds, window->bounds());

  // Ensure that |browser| is in a normal show state.
  auto* window_state = WindowState::Get(window);
  window_state->Restore();
  ASSERT_TRUE(window_state->IsNormalStateType());

  // Turn on session restore.
  SessionStartupPref::SetStartupPref(
      profile(), SessionStartupPref(SessionStartupPref::LAST));
}

// Tests that Full Restore data overrides the browser's session restore data.
// Session restore is turned on in the PRE phase of the test, simulating a user
// logging out and back in.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       FullRestoreOverridesSessionRestoreTest) {
  PrefService* local_state = g_browser_process->local_state();
  static_cast<PrefRegistrySimple*>(local_state->DeprecatedGetPrefRegistry())
      ->RegisterIntegerPref(kRestoreIdPrefName, 0);
  const SessionID::id_type previous_browser_id =
      static_cast<SessionID::id_type>(
          local_state->GetInteger(kRestoreIdPrefName));
  ASSERT_NE(0, previous_browser_id);

  auto* browser_list = BrowserList::GetInstance();
  // Initially there should not be any browsers.
  ASSERT_TRUE(browser_list->empty());

  // Create Full Restore launch data before launching any browser, simulating
  // Full Restore data being saved prior to restart.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(),
      std::make_unique<::app_restore::AppLaunchInfo>(
          app_constants::kChromeAppId, previous_browser_id));
  CreateAndSaveWindowInfo(
      kDeskId, kDeskUuid, kCurrentBounds, chromeos::WindowStateType::kNormal,
      ui::SHOW_STATE_DEFAULT, previous_browser_id, /*snap_percentage=*/0);
  WaitForAppLaunchInfoSaved();

  // Launch the browser.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  SetShouldRestore(app_launch_handler.get());

  ASSERT_EQ(1u, browser_list->size());

  // The restored browser's bounds should be the bounds saved by Full Restore,
  // i.e. |kCurrentBounds|.
  const gfx::Rect& browser_bounds =
      browser_list->get(0u)->window()->GetNativeWindow()->bounds();
  EXPECT_EQ(kCurrentBounds, browser_bounds);
}

// Test Lacros window properties and bounds are restored correctly.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       RestoreLacrosWindowProperties) {
  gfx::Size size(32, 32);
  gfx::Point origin(100, 100);
  gfx::Rect prerestore_bounds(origin, size);

  // Create Full Restore launch data before launching any browser, simulating
  // Full Restore data being saved prior to restart. `kWindowId1` is the restore
  // window id.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kLacrosAppId, kWindowId1));
  CreateAndSaveWindowInfo(
      kDeskId, kDeskUuid, prerestore_bounds, chromeos::WindowStateType::kNormal,
      ui::SHOW_STATE_DEFAULT, kWindowId1, /*snap_percentage=*/0);
  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler, and set should restore to save the Full
  // Restore data.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // Create a WMHelper instance for Surface to set in the constructor.
  std::unique_ptr<exo::WMHelper> wm_helper = std::make_unique<exo::WMHelper>();

  // Create the Lacros window surface and restore it.
  auto shell_surface =
      exo::test::ShellSurfaceBuilder(size).SetNoCommit().BuildShellSurface();
  shell_surface->SetRestoreInfo(/*restore_session_id=*/kWindowId2,
                                /*restore_window_id=*/kWindowId1);
  shell_surface->root_surface()->Commit();

  EXPECT_EQ(kWindowId2,
            shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                ::app_restore::kWindowIdKey));
  EXPECT_EQ(kWindowId1,
            shell_surface->GetWidget()->GetNativeWindow()->GetProperty(
                ::app_restore::kRestoreWindowIdKey));
  EXPECT_EQ(prerestore_bounds,
            shell_surface->GetWidget()->GetNativeWindow()->GetBoundsInScreen());
}

// Launch a desk template with a browser after full restore.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerBrowserTest,
                       LaunchDeskTemplateAfterFullRestore) {
  // Add the chrome browser launch info.
  ::full_restore::SaveAppLaunchInfo(
      profile()->GetPath(), std::make_unique<::app_restore::AppLaunchInfo>(
                                app_constants::kChromeAppId, kWindowId1));

  auto app_launch_info = std::make_unique<::app_restore::AppLaunchInfo>(
      app_constants::kChromeAppId, kWindowId2);
  app_launch_info->app_type_browser = true;
  ::full_restore::SaveAppLaunchInfo(profile()->GetPath(),
                                    std::move(app_launch_info));

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  app_launch_handler->LaunchBrowserWhenReady(/*first_run_full_restore=*/false);
  content::RunAllTasksUntilIdle();

  // Verify there is new browser launched.
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  Browser* browser_from_full_restore = BrowserList::GetInstance()->get(0);

  // We're now going to create a new desk and a browser in that desk.
  AutotestDesksApi().CreateNewDesk();
  ActivateDesk(/*index=*/1);

  const gfx::Rect expected_bounds(10, 10, 500, 300);
  const GURL expected_url("https://example.org");

  Browser* new_browser = Browser::Create(
      Browser::CreateParams(Browser::TYPE_NORMAL, profile(), false));

  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.StartWatchingNewWebContents();
  chrome::AddTabAt(new_browser, expected_url, /*index=*/-1,
                   /*foreground=*/true);
  navigation_observer.Wait();

  new_browser->window()->Show();
  new_browser->window()->SetBounds(expected_bounds);

  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);

  // The browser has now been created. We're now going to enter overview mode
  // and save the desk as a template. Once saved, we'll exit overview mode.
  ToggleOverview();
  WaitForOverviewEnterAnimation();

  ClickSaveDeskAsTemplateButton();

  ToggleOverview();
  WaitForOverviewExitAnimation();

  ASSERT_FALSE(Shell::Get()->overview_controller()->overview_session());

  // Move the browser a bit and then close it. This is to make sure that when we
  // create a new browser, its bounds are actually coming from the template.
  new_browser->window()->SetBounds(expected_bounds + gfx::Vector2d(10, 10));
  web_app::CloseAndWait(new_browser);

  ASSERT_EQ(BrowserList::GetInstance()->size(), 1u);

  // We're now going to launch the template and verify that we have a new
  // browser, and that it has the correct bounds and URL.
  ToggleOverview();
  WaitForOverviewEnterAnimation();

  // Enter the saved desk library.
  ClickButton(GetExpandedStateLibraryButton());
  // Launch the first entry.
  ClickTemplateItem(/*index=*/0);

  ToggleOverview();
  WaitForOverviewExitAnimation();

  ASSERT_EQ(BrowserList::GetInstance()->size(), 2u);

  Browser* browser_from_template = nullptr;
  for (Browser* b : *BrowserList::GetInstance()) {
    if (b != browser_from_full_restore) {
      browser_from_template = b;
      break;
    }
  }

  ASSERT_TRUE(browser_from_template);
  // Verify that the browser has the same bounds as was captured.
  EXPECT_EQ(expected_bounds, browser_from_template->window()->GetBounds());
  // Verify that the browser has a tab with the expected URL.
  EXPECT_EQ(1, browser_from_template->tab_strip_model()->count());
  EXPECT_EQ(expected_url, browser_from_template->tab_strip_model()
                              ->GetWebContentsAt(0)
                              ->GetVisibleURL());
  // Verify that the browser window has a negative restore window ID (and lower
  // than the special value -1).
  EXPECT_LT(browser_from_template->window()->GetNativeWindow()->GetProperty(
                ::app_restore::kRestoreWindowIdKey),
            -1);
}

class FullRestoreAppLaunchHandlerChromeAppBrowserTest
    : public FullRestoreAppLaunchHandlerBrowserTest {
 public:
  FullRestoreAppLaunchHandlerChromeAppBrowserTest() {
    ResetRestoreForTesting();
    set_launch_browser_for_testing(
        std::make_unique<ScopedLaunchBrowserForTesting>());
  }
  ~FullRestoreAppLaunchHandlerChromeAppBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerChromeAppBrowserTest,
                       RestoreChromeApp) {
  // Have 4 desks total.
  AutotestDesksApi().CreateNewDesk();
  AutotestDesksApi().CreateNewDesk();
  AutotestDesksApi().CreateNewDesk();
  ActivateDesk(2);

  ::full_restore::SetActiveProfilePath(profile()->GetPath());

  // Create the restore data.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);
  SaveChromeAppLaunchInfo(extension->id());

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);

  auto* window = app_window->GetNativeWindow();
  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  // Simulate the system shutdown process, and the window is closed.
  FullRestoreService::GetForProfile(profile())->OnAppTerminating();
  CloseAppWindow(app_window);
  WaitForAppLaunchInfoSaved();

  // Create a non-restored window in the restored window's desk container.
  Browser::CreateParams non_restored_params(profile(), true);
  non_restored_params.initial_workspace = "2";
  Browser* non_restored_browser = Browser::Create(non_restored_params);
  AddBlankTabAndShow(non_restored_browser);
  aura::Window* non_restored_window =
      non_restored_browser->window()->GetNativeWindow();

  // Read from the restore data.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // Verify the restore window id.
  app_window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window);

  window = app_window->GetNativeWindow();
  ASSERT_TRUE(window);
  int restore_window_id =
      window->GetProperty(::app_restore::kRestoreWindowIdKey);
  EXPECT_NE(0, restore_window_id);

  auto* window_info = window->GetProperty(::app_restore::kWindowInfoKey);
  ASSERT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  int32_t* index = window->GetProperty(::app_restore::kActivationIndexKey);
  ASSERT_TRUE(index);
  EXPECT_EQ(kActivationIndex, *index);
  EXPECT_EQ(kDeskId, window->GetProperty(aura::client::kWindowWorkspaceKey));

  // Non-topmost windows created from full restore are not activated. They will
  // become activatable after a couple seconds. Verify that the
  // `non_restored_window` is topmost and check that `window` is not
  // activatable.
  std::vector<aura::Window*> expected_stacking{window, non_restored_window};
  EXPECT_EQ(non_restored_window->parent()->children(), expected_stacking);
  EXPECT_FALSE(views::Widget::GetWidgetForNativeView(window)->IsActive());
  EXPECT_FALSE(wm::CanActivateWindow(window));

  // Wait a couple seconds and verify the window is now activatable.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Seconds(3));
  run_loop.Run();

  EXPECT_TRUE(wm::CanActivateWindow(window));

  EXPECT_EQ(0, ::app_restore::FetchRestoreWindowId(extension->id()));

  // Close the window.
  CloseAppWindow(app_window);
  ASSERT_FALSE(GetWindowInfo(restore_window_id));

  // Remove the added desks.
  RemoveInactiveDesks();
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerChromeAppBrowserTest,
                       RestoreMinimizedChromeApp) {
  ::full_restore::SetActiveProfilePath(profile()->GetPath());

  // Create the restore data.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);
  SaveChromeAppLaunchInfo(extension->id());

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);

  // Save app window as minimized.
  SaveWindowInfo(app_window->GetNativeWindow(), 1u,
                 chromeos::WindowStateType::kMinimized);

  WaitForAppLaunchInfoSaved();

  // Read from the restore data.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // Tests that the created window is minimized.
  app_window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window);
  EXPECT_TRUE(app_window->GetBaseWindow()->IsMinimized());
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerChromeAppBrowserTest,
                       RestoreMultipleChromeAppWindows) {
  ::full_restore::SetActiveProfilePath(profile()->GetPath());

  // Create the restore data, 2 windows for 1 chrome app.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);
  const std::string& app_id = extension->id();
  SaveChromeAppLaunchInfo(app_id);

  extensions::AppWindow* app_window1 = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window1);
  auto* window1 = app_window1->GetNativeWindow();
  SaveWindowInfo(window1);

  SaveChromeAppLaunchInfo(app_id);

  extensions::AppWindow* app_window2 = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window2);
  auto* window2 = app_window2->GetNativeWindow();
  SaveWindowInfo(window2);

  WaitForAppLaunchInfoSaved();

  // Read from the restore data.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // Verify the restore window id;
  app_window1 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window1);
  window1 = app_window1->GetNativeWindow();
  ASSERT_TRUE(window1);
  EXPECT_NE(0, window1->GetProperty(::app_restore::kRestoreWindowIdKey));

  auto window_info = GetWindowInfo(window1);
  ASSERT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MAX, window_info->activation_index.value());

  app_window2 = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window2);
  window2 = app_window2->GetNativeWindow();
  ASSERT_TRUE(window2);
  EXPECT_NE(0, window2->GetProperty(::app_restore::kRestoreWindowIdKey));

  window_info = GetWindowInfo(window2);
  ASSERT_TRUE(window_info);
  EXPECT_TRUE(window_info->activation_index.has_value());
  EXPECT_EQ(INT32_MAX, window_info->activation_index.value());

  // Create a new window, verity the restore window id is 0.
  auto* app_window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window);
  auto* window = app_window->GetNativeWindow();
  ASSERT_TRUE(window);
  EXPECT_EQ(0, window->GetProperty(::app_restore::kRestoreWindowIdKey));

  // Close the window.
  CloseAppWindow(app_window1);
  CloseAppWindow(app_window2);
}

// Tests that fullscreened windows will not be restored as fullscreen, which is
// not supported for full restore. Regression test for
// https://crbug.com/1203010.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerChromeAppBrowserTest,
                       ImmersiveFullscreenApp) {
  ::full_restore::SetActiveProfilePath(profile()->GetPath());

  // Create the restore data.
  const extensions::Extension* extension =
      LoadAndLaunchPlatformApp("launch", "Launched");
  ASSERT_TRUE(extension);
  SaveChromeAppLaunchInfo(extension->id());

  extensions::AppWindow* app_window = CreateAppWindow(profile(), extension);
  ASSERT_TRUE(app_window);

  // Toggle immersive fullscreen by simulating what happens when F4 is pressed.
  // WindowRestoreController will save to file when the state changes.
  const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(app_window->GetNativeWindow())->OnWMEvent(&event);

  WaitForAppLaunchInfoSaved();

  // Read from the restore data.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  // Tests that the created window is not fullscreen.
  app_window = CreateAppWindow(browser()->profile(), extension);
  ASSERT_TRUE(app_window);
  EXPECT_FALSE(app_window->GetBaseWindow()->IsFullscreenOrPending());
}

class FullRestoreAppLaunchHandlerArcAppBrowserTest
    : public FullRestoreAppLaunchHandlerBrowserTest {
 public:
  FullRestoreAppLaunchHandlerArcAppBrowserTest() = default;
  FullRestoreAppLaunchHandlerArcAppBrowserTest(
      const FullRestoreAppLaunchHandlerArcAppBrowserTest&) = delete;
  FullRestoreAppLaunchHandlerArcAppBrowserTest& operator=(
      const FullRestoreAppLaunchHandlerArcAppBrowserTest&) = delete;
  ~FullRestoreAppLaunchHandlerArcAppBrowserTest() override = default;

  // FullRestoreAppLaunchHandlerBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    arc_helper_.SetUpCommandLine(command_line);
    FullRestoreAppLaunchHandlerBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpInProcessBrowserTestFixture() override {
    arc_helper_.SetUpInProcessBrowserTestFixture();
    FullRestoreAppLaunchHandlerBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    arc_helper_.SetUpOnMainThread(profile());
    FullRestoreAppLaunchHandlerBrowserTest::SetUpOnMainThread();
  }

 protected:
  arc::mojom::AppHost* app_host() { return arc_helper_.GetAppHost(); }

  void SetProfile() {
    ::full_restore::FullRestoreSaveHandler::GetInstance()
        ->SetPrimaryProfilePath(profile()->GetPath());
    ::full_restore::SetActiveProfilePath(profile()->GetPath());
    test_app_restore_info_observer_.Reset();
  }

  void SaveAppLaunchInfo(const std::string& app_id, int32_t session_id) {
    ::full_restore::SaveAppLaunchInfo(
        profile()->GetPath(),
        std::make_unique<::app_restore::AppLaunchInfo>(
            app_id, ui::EF_NONE, session_id, display::kDefaultDisplayId));
  }

  void Restore() {
    test_app_restore_info_observer_.Reset();

    auto* arc_task_handler =
        app_restore::AppRestoreArcTaskHandler::GetForProfile(profile());
    ASSERT_TRUE(arc_task_handler);

    arc_app_queue_restore_handler_ =
        arc_task_handler->GetFullRestoreArcAppQueueRestoreHandler();
    DCHECK(arc_app_queue_restore_handler_);
    arc_app_queue_restore_handler_->is_app_connection_ready_ = false;

    app_launch_handler_ =
        std::make_unique<FullRestoreAppLaunchHandler>(profile());
    SetShouldRestore(app_launch_handler_.get());
  }

  void ForceLaunchApp(const std::string& app_id, int32_t window_id) {
    if (arc_app_queue_restore_handler_) {
      arc_app_queue_restore_handler_->LaunchAppWindow(app_id, window_id);
      content::RunAllTasksUntilIdle();
    }
  }

  void VerifyGetArcAppLaunchInfo(const std::string& app_id,
                                 int32_t session_id,
                                 int32_t restore_window_id) {
    auto app_launch_info = GetArcAppLaunchInfo(app_id, session_id);
    ASSERT_TRUE(app_launch_info);

    EXPECT_EQ(app_id, app_launch_info->app_id);

    EXPECT_TRUE(app_launch_info->window_id.has_value());
    EXPECT_EQ(restore_window_id, app_launch_info->window_id.value());

    EXPECT_TRUE(app_launch_info->event_flag.has_value());
    EXPECT_EQ(ui::EF_NONE, app_launch_info->event_flag.value());
  }

  void VerifyWindowProperty(aura::Window* window,
                            int32_t window_id,
                            int32_t restore_window_id,
                            bool hidden) {
    ASSERT_TRUE(window);
    EXPECT_EQ(window_id, window->GetProperty(::app_restore::kWindowIdKey));
    EXPECT_EQ(restore_window_id,
              window->GetProperty(::app_restore::kRestoreWindowIdKey));
    EXPECT_EQ(hidden,
              window->GetProperty(::app_restore::kParentToHiddenContainerKey));
  }

  void VerifyWindowInfo(aura::Window* window,
                        int32_t activation_index,
                        chromeos::WindowStateType window_state_type =
                            chromeos::WindowStateType::kDefault) {
    auto window_info = GetWindowInfo(window);
    ASSERT_TRUE(window_info);
    EXPECT_TRUE(window_info->activation_index.has_value());
    EXPECT_EQ(activation_index, window_info->activation_index.value());

    EXPECT_FALSE(window_info->current_bounds.has_value());

    // For ARC windows, Android can restore window minimized or maximized
    // status, so the WindowStateType from the window info for the minimized and
    // maximized state will be removed.
    if (window_state_type == chromeos::WindowStateType::kMaximized ||
        window_state_type == chromeos::WindowStateType::kMinimized) {
      EXPECT_FALSE(window_info->window_state_type.has_value());
    } else {
      EXPECT_TRUE(window_info->window_state_type.has_value());
      EXPECT_EQ(window_state_type, window_info->window_state_type.value());
    }
  }

  void VerifyObserver(aura::Window* window, int launch_count, int init_count) {
    auto& launched_windows = test_app_restore_info_observer_.launched_windows();
    if (launch_count == 0) {
      EXPECT_TRUE(launched_windows.find(window) == launched_windows.end());
    } else {
      EXPECT_EQ(launch_count, launched_windows[window]);
    }

    auto& initialized_windows =
        test_app_restore_info_observer_.initialized_windows();
    if (init_count == 0) {
      EXPECT_TRUE(initialized_windows.find(window) ==
                  initialized_windows.end());
    } else {
      EXPECT_EQ(init_count, initialized_windows[window]);
    }
  }

  void VerifyThemeColor(const std::string& app_id,
                        int32_t task_id,
                        uint32_t primary_color,
                        uint32_t status_bar_color) {
    const auto& app_id_to_launch_list =
        app_launch_handler()->restore_data()->app_id_to_launch_list();

    auto it = app_id_to_launch_list.find(app_id);
    EXPECT_TRUE(it != app_id_to_launch_list.end());

    auto data_it = it->second.find(task_id);
    EXPECT_TRUE(data_it != it->second.end());

    EXPECT_TRUE(data_it->second->primary_color.has_value());
    EXPECT_EQ(primary_color, data_it->second->primary_color.value());

    EXPECT_TRUE(data_it->second->status_bar_color.has_value());
    EXPECT_EQ(status_bar_color, data_it->second->status_bar_color.value());
  }

  void VerifyRestoreData(const std::string& app_id, int32_t window_id) {
    DCHECK(app_launch_handler());

    const auto& app_id_to_launch_list =
        app_launch_handler()->restore_data()->app_id_to_launch_list();

    auto it = app_id_to_launch_list.find(app_id);
    EXPECT_TRUE(it != app_id_to_launch_list.end());

    auto data_it = it->second.find(window_id);
    EXPECT_TRUE(data_it != it->second.end());
  }

  void VerifyNoRestoreData(const std::string& app_id) {
    DCHECK(app_launch_handler());

    const auto& app_id_to_launch_list =
        app_launch_handler()->restore_data()->app_id_to_launch_list();

    auto it = app_id_to_launch_list.find(app_id);
    EXPECT_FALSE(it != app_id_to_launch_list.end());
  }

  FullRestoreAppLaunchHandler* app_launch_handler() {
    if (!app_launch_handler_)
      app_launch_handler_ =
          std::make_unique<FullRestoreAppLaunchHandler>(profile());
    return app_launch_handler_.get();
  }

  TestAppRestoreInfoObserver* test_app_restore_info_observer() {
    return &test_app_restore_info_observer_;
  }

  const std::map<int32_t, std::string>& task_id_to_app_id() {
    auto* arc_save_handler =
        ::full_restore::FullRestoreSaveHandler::GetInstance()
            ->arc_save_handler_.get();
    DCHECK(arc_save_handler);
    return arc_save_handler->task_id_to_app_id_;
  }

 protected:
  raw_ptr<app_restore::ArcAppQueueRestoreHandler, ExperimentalAsh>
      arc_app_queue_restore_handler_ = nullptr;
  AppRestoreArcTestHelper arc_helper_;

 private:
  std::unique_ptr<FullRestoreAppLaunchHandler> app_launch_handler_;
  TestAppRestoreInfoObserver test_app_restore_info_observer_;
};

// Test the not restored ARC window is not added to the hidden container.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       NotHideArcWindow) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id1);

  // Create the window for app1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/0);
  VerifyWindowProperty(window, kTaskId1, /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Simulate creating the task.
  arc_helper_.CreateTask(app_id, kTaskId1, session_id1);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/0);

  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  Restore();
  widget->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);

  int32_t session_id2 = 1;

  // Create the window to simulate launching the ARC app.
  int32_t kTaskId2 = 200;
  auto* widget1 = CreateExoWindow("org.chromium.arc.200");
  auto* window1 = widget1->GetNativeWindow();

  // The task is not ready, so the window is currently in a hidden container.
  EXPECT_EQ(Shell::GetContainer(window1->GetRootWindow(),
                                kShellWindowId_UnparentedContainer),
            window1->parent());

  VerifyObserver(window1, /*launch_count=*/0, /*init_count=*/1);
  VerifyWindowProperty(window1, kTaskId2,
                       ::app_restore::kParentToHiddenContainer,
                       /*hidden=*/true);

  // Simulate creating the task for the ARC app window.
  arc_helper_.CreateTask(app_id, kTaskId2, session_id2);

  VerifyObserver(window1, /*launch_count=*/0, /*init_count=*/1);
  VerifyWindowProperty(window1, kTaskId2,
                       ::app_restore::kParentToHiddenContainer,
                       /*hidden=*/false);

  int32_t session_id3 = 2;
  int32_t kTaskId3 = 300;
  // Simulate creating the task before the window is created.
  arc_helper_.CreateTask(app_id, kTaskId3, session_id3);

  // Create the window to simulate launching the ARC app.
  auto* widget2 = CreateExoWindow("org.chromium.arc.300");
  auto* window2 = widget2->GetNativeWindow();

  VerifyObserver(window2, /*launch_count=*/0, /*init_count=*/0);
  // The window should not be hidden.
  VerifyWindowProperty(window2, kTaskId3,
                       /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Destroy the task and close the window.
  app_host()->OnTaskDestroyed(kTaskId2);
  widget1->CloseNow();
  app_host()->OnTaskDestroyed(kTaskId3);
  widget2->CloseNow();

  ::app_restore::AppRestoreInfo::GetInstance()->RemoveObserver(
      test_app_restore_info_observer());
  arc_helper_.StopInstance();
}

// Test restoration when the ARC window is created before OnTaskCreated is
// called.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       RestoreArcApp) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id1);

  // Create the window for app1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/0);
  VerifyWindowProperty(window, kTaskId1, /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Simulate creating the task.
  arc_helper_.CreateTask(app_id, kTaskId1, session_id1);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/0);

  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  // Simulate the system shutdown process, and the window is closed.
  FullRestoreService::GetForProfile(profile())->OnAppTerminating();
  widget->CloseNow();
  WaitForAppLaunchInfoSaved();

  Restore();

  app_host()->OnTaskDestroyed(kTaskId1);

  int32_t session_id2 =
      ::app_restore::kArcSessionIdOffsetForRestoredLaunching + 1;

  // Create some desks so we can test that the exo window is placed in the
  // correct desk container after the task is created.
  AutotestDesksApi().CreateNewDesk();
  AutotestDesksApi().CreateNewDesk();
  AutotestDesksApi().CreateNewDesk();

  ForceLaunchApp(app_id, kTaskId1);

  // Create the window to simulate the restoration for the app. The task id
  // needs to match the |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId2 = 200;
  widget = CreateExoWindow("org.chromium.arc.200");
  window = widget->GetNativeWindow();

  // The task is not ready, so the window is currently in a hidden container.
  EXPECT_EQ(Shell::GetContainer(window->GetRootWindow(),
                                kShellWindowId_UnparentedContainer),
            window->parent());

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/1);
  VerifyWindowProperty(window, kTaskId2,
                       ::app_restore::kParentToHiddenContainer,
                       /*hidden=*/true);

  // Simulate creating the task for the restored window.
  arc_helper_.CreateTask(app_id, kTaskId2, session_id2);

  // Tests that after the task is created, the window is placed in the container
  // associated with `kDeskId` (2), which is desk C.
  EXPECT_EQ(Shell::GetContainer(window->GetRootWindow(),
                                kShellWindowId_DeskContainerC),
            window->parent());

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/1);
  VerifyWindowProperty(window, kTaskId2, kTaskId1, /*hidden=*/false);
  VerifyWindowInfo(window, kActivationIndex);

  // Destroy the task and close the window.
  app_host()->OnTaskDestroyed(kTaskId2);
  widget->CloseNow();

  ASSERT_FALSE(GetWindowInfo(kTaskId1));

  ::app_restore::AppRestoreInfo::GetInstance()->RemoveObserver(
      test_app_restore_info_observer());
  arc_helper_.StopInstance();

  RemoveInactiveDesks();
}

// Test restoration when the ARC ghost window is created before OnTaskCreated is
// called.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       RestoreArcGhostWindow) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id1);

  // Create the window for app1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/0);
  VerifyWindowProperty(window, kTaskId1, /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Simulate creating the task.
  arc_helper_.CreateTask(app_id, kTaskId1, session_id1);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/0);

  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  Restore();
  widget->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);

  int32_t session_id2 = ::app_restore::CreateArcSessionId();
  ::full_restore::FullRestoreReadHandler::GetInstance()
      ->SetArcSessionIdForWindowId(session_id2, kTaskId1);

  // Create the window with the ghost window session to simulate the ghost
  // window restoration for the app.
  widget = CreateExoWindow(
      base::StringPrintf("org.chromium.arc.session.%d", session_id2), app_id);
  window = widget->GetNativeWindow();

  SaveAppLaunchInfo(app_id, session_id2);

  // The ghost window should not be hidden.
  VerifyWindowProperty(window, /*window_id*/ 0,
                       /*restore_window_id*/ kTaskId1,
                       /*hidden=*/false);

  VerifyGetArcAppLaunchInfo(app_id, session_id2, kTaskId1);

  // Call SaveAppLaunchInfo to simulate the ARC app is ready, and launch the app
  // again.
  SaveAppLaunchInfo(app_id, session_id2);

  // Simulate creating the task for the restored window.
  int32_t kTaskId2 = 200;
  window->SetProperty(::app_restore::kWindowIdKey, kTaskId2);
  arc_helper_.CreateTask(app_id, kTaskId2, session_id2);

  VerifyWindowProperty(window, kTaskId2, kTaskId1, /*hidden=*/false);
  VerifyWindowInfo(window, kActivationIndex);

  // Verify the ghost window session id has been removed from the restore data.
  EXPECT_FALSE(GetArcAppLaunchInfo(app_id, session_id2));

  // Destroy the task and close the window.
  app_host()->OnTaskDestroyed(kTaskId2);
  widget->CloseNow();

  ::app_restore::AppRestoreInfo::GetInstance()->RemoveObserver(
      test_app_restore_info_observer());
  arc_helper_.StopInstance();

  RemoveInactiveDesks();
}

// Test the ARC ghost window is saved if the task is not created.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       SaveArcGhostWindow) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id1);

  // Create the window for app1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/0);
  VerifyWindowProperty(window, kTaskId1, /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Simulate creating the task.
  arc_helper_.CreateTask(app_id, kTaskId1, session_id1);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/0);

  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  // Simulate the system reboot.
  Restore();
  widget->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);

  int32_t session_id2 = ::app_restore::CreateArcSessionId();
  ::full_restore::FullRestoreReadHandler::GetInstance()
      ->SetArcSessionIdForWindowId(session_id2, kTaskId1);

  // Create the window with the ghost window session to simulate the ghost
  // window restoration for the app.
  widget = CreateExoWindow(
      base::StringPrintf("org.chromium.arc.session.%d", session_id2), app_id);
  window = widget->GetNativeWindow();

  SaveAppLaunchInfo(app_id, session_id2);
  SaveWindowInfo(window);

  // The ghost window should not be hidden.
  VerifyWindowProperty(window, /*window_id*/ 0,
                       /*restore_window_id*/ kTaskId1,
                       /*hidden=*/false);

  VerifyGetArcAppLaunchInfo(app_id, session_id2, kTaskId1);

  WaitForAppLaunchInfoSaved();

  // Simulate the system reboot before the task id is created.
  Restore();
  widget->CloseNow();

  int32_t session_id3 = ::app_restore::CreateArcSessionId();
  ::full_restore::FullRestoreReadHandler::GetInstance()
      ->SetArcSessionIdForWindowId(session_id3, session_id2);

  // Create the window with the ghost window session to simulate the ghost
  // window restoration for the app.
  widget = CreateExoWindow(
      base::StringPrintf("org.chromium.arc.session.%d", session_id3), app_id);
  window = widget->GetNativeWindow();

  SaveAppLaunchInfo(app_id, session_id3);
  SaveWindowInfo(window);

  // Call SaveAppLaunchInfo to simulate the ARC app is ready, and launch the app
  // again.
  SaveAppLaunchInfo(app_id, session_id3);

  // Simulate creating the task for the restored window.
  int32_t kTaskId2 = 200;
  arc_helper_.CreateTask(app_id, kTaskId2, session_id3);
  window->SetProperty(::app_restore::kWindowIdKey, kTaskId2);

  VerifyWindowProperty(window, kTaskId2, session_id2, /*hidden=*/false);
  VerifyWindowInfo(window, kActivationIndex);

  // Verify the ghost window session id has been removed from the restore data.
  EXPECT_FALSE(GetArcAppLaunchInfo(app_id, session_id3));

  // Destroy the task and close the window.
  app_host()->OnTaskDestroyed(kTaskId2);
  widget->CloseNow();

  ::app_restore::AppRestoreInfo::GetInstance()->RemoveObserver(
      test_app_restore_info_observer());
  arc_helper_.StopInstance();

  RemoveInactiveDesks();
}

// Test restoration with multiple ARC apps, when the ARC windows are created
// before and after OnTaskCreated is called.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       RestoreMultipleArcApps) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1 and store its bounds.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();
  gfx::Rect pre_restore_bounds_1 = window1->GetBoundsInScreen();

  // Create the window for the app2 and store its bounds. The task id needs to
  // match the |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId2 = 101;
  views::Widget* widget2 = CreateExoWindow("org.chromium.arc.101");
  aura::Window* window2 = widget2->GetNativeWindow();
  gfx::Rect pre_restore_bounds_2 = window2->GetBoundsInScreen();

  // Simulate creating kTaskId2.
  arc_helper_.CreateTask(app_id2, kTaskId2, session_id2);
  VerifyObserver(window1, /*launch_count=*/1, /*init_count=*/0);
  VerifyObserver(window2, /*launch_count=*/1, /*init_count=*/0);

  VerifyWindowProperty(window1, kTaskId1, /*restore_window_id*/ 0,
                       /*hidden=*/false);
  VerifyWindowProperty(window2, kTaskId2, /*restore_window_id*/ 0,
                       /*hidden=*/false);

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  int32_t activation_index2 = 12;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kMaximized);
  SaveWindowInfo(window2, activation_index2,
                 chromeos::WindowStateType::kMinimized);

  WaitForAppLaunchInfoSaved();

  Restore();
  widget1->CloseNow();
  widget2->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);
  app_host()->OnTaskDestroyed(kTaskId2);

  ForceLaunchApp(app_id1, kTaskId1);
  ForceLaunchApp(app_id2, kTaskId2);

  int32_t session_id3 =
      ::app_restore::kArcSessionIdOffsetForRestoredLaunching + 1;
  int32_t session_id4 =
      ::app_restore::kArcSessionIdOffsetForRestoredLaunching + 2;

  // Create the window to simulate the restoration for the app1. The task id
  // needs to match the |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId3 = 201;
  widget1 = CreateExoWindow("org.chromium.arc.201");
  window1 = widget1->GetNativeWindow();

  VerifyWindowProperty(window1, kTaskId3,
                       ::app_restore::kParentToHiddenContainer,
                       /*hidden=*/true);
  EXPECT_EQ(pre_restore_bounds_1, window1->GetBoundsInScreen());

  // Simulate creating tasks for apps.
  arc_helper_.CreateTask(app_id1, kTaskId3, session_id3);

  int32_t kTaskId4 = 202;
  arc_helper_.CreateTask(app_id2, kTaskId4, session_id4);

  // Create the window to simulate the restoration for the app2.
  widget2 = CreateExoWindow("org.chromium.arc.202");
  window2 = widget2->GetNativeWindow();
  EXPECT_EQ(pre_restore_bounds_2, window2->GetBoundsInScreen());

  VerifyObserver(window1, /*launch_count=*/1, /*init_count=*/1);
  VerifyObserver(window2, /*launch_count=*/1, /*init_count=*/1);
  VerifyWindowProperty(window1, kTaskId3, kTaskId1, /*hidden=*/false);
  VerifyWindowProperty(window2, kTaskId4, kTaskId2, /*hidden=*/false);
  VerifyWindowInfo(window1, activation_index1,
                   chromeos::WindowStateType::kMaximized);
  VerifyWindowInfo(window2, activation_index2,
                   chromeos::WindowStateType::kMinimized);

  // destroy tasks and close windows.
  widget1->CloseNow();
  app_host()->OnTaskDestroyed(kTaskId3);
  app_host()->OnTaskDestroyed(kTaskId4);
  widget2->CloseNow();

  ASSERT_FALSE(GetWindowInfo(kTaskId1));
  ASSERT_FALSE(GetWindowInfo(kTaskId2));

  arc_helper_.StopInstance();
}

// Test ARC apps restore data is removed, when Play Store is disabled.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       DisablePlayStore) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1 and store its bounds.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  // Create the window for the app2 and store its bounds.
  int32_t kTaskId2 = 101;
  views::Widget* widget2 = CreateExoWindow("org.chromium.arc.101");
  aura::Window* window2 = widget2->GetNativeWindow();

  // Simulate creating kTaskId2.
  arc_helper_.CreateTask(app_id2, kTaskId2, session_id2);
  VerifyObserver(window1, /*launch_count=*/1, /*init_count=*/0);
  VerifyObserver(window2, /*launch_count=*/1, /*init_count=*/0);

  VerifyWindowProperty(window1, kTaskId1, /*restore_window_id*/ 0,
                       /*hidden=*/false);
  VerifyWindowProperty(window2, kTaskId2, /*restore_window_id*/ 0,
                       /*hidden=*/false);

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  int32_t activation_index2 = 12;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kMaximized);
  SaveWindowInfo(window2, activation_index2,
                 chromeos::WindowStateType::kMinimized);

  WaitForAppLaunchInfoSaved();

  // Verify ARC app launch info is saved in `restore_data`.
  const auto* restore_data =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetRestoreData(
          profile()->GetPath());
  ASSERT_TRUE(restore_data);
  ASSERT_FALSE(restore_data->app_id_to_launch_list().empty());
  ASSERT_FALSE(task_id_to_app_id().empty());

  // Simulate Play Store is disabled.
  app_restore::AppRestoreArcTaskHandler::GetForProfile(profile())
      ->OnArcPlayStoreEnabledChanged(/*enabled=*/false);
  widget1->CloseNow();

  // Verify ARC app launch info is removed from `restore_data`.
  ASSERT_TRUE(restore_data->app_id_to_launch_list().empty());
  ASSERT_TRUE(task_id_to_app_id().empty());

  widget2->CloseNow();

  // Simulate Play Store is enabled and `app_id1` is launched.
  int32_t session_id3 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t kTaskId3 = 201;
  SaveAppLaunchInfo(app_id1, session_id3);
  arc_helper_.CreateTask(app_id1, kTaskId3, session_id3);
  ASSERT_FALSE(restore_data->app_id_to_launch_list().empty());
  ASSERT_TRUE(base::Contains(restore_data->app_id_to_launch_list(), app_id1));

  arc_helper_.StopInstance();
}

// Test restoration when the ARC window is created before OnTaskCreated is
// called.
IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       ArcAppThemeColorUpdate) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id);

  // Create the window for app1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId = 100;
  uint32_t kPrimaryColor = 0xFFFFFFFF;
  uint32_t kStatusBarColor = 0xFF000000;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/0);
  VerifyWindowProperty(window, kTaskId, /*restore_window_id=*/0,
                       /*hidden=*/false);

  // Simulate creating the task.
  arc_helper_.CreateTask(app_id, kTaskId, session_id);
  arc_helper_.UpdateThemeColor(kTaskId, kPrimaryColor, kStatusBarColor);

  VerifyObserver(window, /*launch_count=*/1, /*init_count=*/0);

  SaveWindowInfo(window);

  WaitForAppLaunchInfoSaved();

  ASSERT_TRUE(app_launch_handler());
  content::RunAllTasksUntilIdle();

  VerifyThemeColor(app_id, kTaskId, kPrimaryColor, kStatusBarColor);

  widget->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId);

  ::app_restore::AppRestoreInfo::GetInstance()->RemoveObserver(
      test_app_restore_info_observer());
  arc_helper_.StopInstance();
}

IN_PROC_BROWSER_TEST_F(FullRestoreAppLaunchHandlerArcAppBrowserTest,
                       DeskTemplateAfterFullRestoreArcApp) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, false);

  const std::string app_id = GetTestApp1Id(kTestAppPackage);
  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  ::app_restore::AppRestoreInfo::GetInstance()->AddObserver(
      test_app_restore_info_observer());

  SaveAppLaunchInfo(app_id, session_id1);

  // Create the window for app1. The task id needs to match the `window_app_id`
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  views::Widget* widget = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window = widget->GetNativeWindow();

  // Simulate creating the task.
  arc_helper_.CreateTask(app_id, kTaskId1, session_id1);

  SaveWindowInfo(window);
  WaitForAppLaunchInfoSaved();

  // Simulate the system shutdown process, and the window is closed.
  widget->CloseNow();

  Restore();

  app_host()->OnTaskDestroyed(kTaskId1);

  int32_t session_id2 =
      ::app_restore::kArcSessionIdOffsetForRestoredLaunching + 1;

  // Create some desks so we can test that the exo window is placed in the
  // correct desk container after the task is created.
  AutotestDesksApi().CreateNewDesk();
  AutotestDesksApi().CreateNewDesk();
  AutotestDesksApi().CreateNewDesk();

  ForceLaunchApp(app_id, kTaskId1);

  // Create the window to simulate the restoration for the app. The task id
  // needs to match the |window_app_id| arg of CreateExoWindow.
  int32_t kTaskId2 = 200;
  widget = CreateExoWindow("org.chromium.arc.200");
  window = widget->GetNativeWindow();

  // The task is not ready, so the window is currently in a hidden container.
  EXPECT_EQ(Shell::GetContainer(window->GetRootWindow(),
                                kShellWindowId_UnparentedContainer),
            window->parent());

  VerifyObserver(window, /*launch_count=*/0, /*init_count=*/1);
  VerifyWindowProperty(window, kTaskId2,
                       ::app_restore::kParentToHiddenContainer,
                       /*hidden=*/true);

  // Simulate creating the task for the restored window.
  arc_helper_.CreateTask(app_id, kTaskId2, session_id2);

  // Activate the most recently created desk.
  ActivateDesk(/*index=*/2);

  // Capture the active desk as a template.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  ClickSaveDeskAsTemplateButton();
  ToggleOverview();
  WaitForOverviewExitAnimation();

  // Destroy the task and close the window.
  app_host()->OnTaskDestroyed(kTaskId2);
  widget->CloseNow();

  // Launch the template.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  ClickButton(GetExpandedStateLibraryButton());
  ClickTemplateItem(/*index=*/0);
  ToggleOverview();
  WaitForOverviewExitAnimation();

  content::RunAllTasksUntilIdle();

  int32_t session_id3 =
      ::app_restore::kArcSessionIdOffsetForRestoredLaunching + 2;
  int32_t kTaskId3 = 300;

  arc_helper_.CreateTask(app_id, kTaskId3, session_id3);

  widget = CreateExoWindow("org.chromium.arc.300");
  window = widget->GetNativeWindow();

  content::RunAllTasksUntilIdle();

  // Verify that the ARC window has a negative restore window ID (and lower than
  // the special value -1).
  EXPECT_LT(window->GetProperty(::app_restore::kRestoreWindowIdKey), -1);
}

class ArcAppQueueRestoreHandlerArcAppBrowserTest
    : public FullRestoreAppLaunchHandlerArcAppBrowserTest {
 protected:
  void UpdateApp(const std::string& app_id, apps::Readiness readiness) {
    apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kArc, app_id);
    app->readiness = readiness;

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    proxy->AppRegistryCache().OnApps(std::move(deltas), apps::AppType::kArc,
                                     false /* should_notify_initialized */);
  }

  void RemoveApp(const std::string& app_id) {
    apps::AppPtr app = std::make_unique<apps::App>(apps::AppType::kArc, app_id);
    app->readiness = apps::Readiness::kUninstalledByUser;

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    proxy->AppRegistryCache().OnApps(std::move(deltas), apps::AppType::kArc,
                                     false /* should_notify_initialized */);
  }

  bool HasRestoreData() {
    DCHECK(arc_app_queue_restore_handler_);
    return arc_app_queue_restore_handler_->HasRestoreData();
  }

  bool HasRestoreData(const std::string& app_id) {
    DCHECK(arc_app_queue_restore_handler_);

    for (auto it : arc_app_queue_restore_handler_->windows_) {
      if (it.second.app_id == app_id)
        return true;
    }
    for (auto it : arc_app_queue_restore_handler_->no_stack_windows_) {
      if (it.app_id == app_id)
        return true;
    }

    return false;
  }

  std::set<std::string> GetAppIds() {
    DCHECK(arc_app_queue_restore_handler_);
    return arc_app_queue_restore_handler_->app_ids_;
  }

  void OnAppConnectionReady() {
    if (!arc_app_queue_restore_handler_) {
      arc_app_queue_restore_handler_ =
          app_restore::AppRestoreArcTaskHandler::GetForProfile(profile())
              ->GetFullRestoreArcAppQueueRestoreHandler();
    }
    arc_app_queue_restore_handler_->OnAppConnectionReady();
  }

  void VerifyWindows(int32_t index,
                     const std::string& app_id,
                     int32_t window_id) {
    DCHECK(arc_app_queue_restore_handler_);
    auto it = arc_app_queue_restore_handler_->windows_.find(index);
    ASSERT_TRUE(it != arc_app_queue_restore_handler_->windows_.end());
    EXPECT_EQ(app_id, it->second.app_id);
    EXPECT_EQ(window_id, it->second.window_id);
  }

  void VerifyNoStackWindows(const std::string& app_id, int32_t window_id) {
    DCHECK(arc_app_queue_restore_handler_);
    bool found = false;
    for (auto it : arc_app_queue_restore_handler_->no_stack_windows_) {
      if (it.app_id == app_id && it.window_id == window_id) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found);
  }
};

// Verify the saved windows in ArcAppLaunchHandler when apps are removed.
IN_PROC_BROWSER_TEST_F(ArcAppQueueRestoreHandlerArcAppBrowserTest, RemoveApps) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);

  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  // Simulate creating kTaskId2 for the app2.
  int32_t kTaskId2 = 101;
  arc_helper_.CreateTask(app_id2, kTaskId2, session_id2);

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kNormal);

  WaitForAppLaunchInfoSaved();

  base::HistogramTester histogram_tester;
  Restore();
  widget1->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);
  app_host()->OnTaskDestroyed(kTaskId2);

  EXPECT_TRUE(HasRestoreData());
  VerifyWindows(activation_index1, app_id1, kTaskId1);
  VerifyNoStackWindows(app_id2, kTaskId2);
  VerifyRestoreData(app_id1, kTaskId1);
  VerifyRestoreData(app_id2, kTaskId2);

  // Remove `app_id2`, and verify windows and restore data for `app_id2` have
  // been removed.
  RemoveApp(app_id2);
  VerifyWindows(activation_index1, app_id1, kTaskId1);
  EXPECT_FALSE(HasRestoreData(app_id2));
  VerifyRestoreData(app_id1, kTaskId1);
  VerifyNoRestoreData(app_id2);

  OnAppConnectionReady();
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   app_restore::kRestoredAppWindowCountHistogram, 1));

  // Remove app_id1, and verify windows and restore data for `app_id1` have been
  // removed.
  RemoveApp(app_id1);
  EXPECT_FALSE(HasRestoreData(app_id1));
  VerifyNoRestoreData(app_id1);
  VerifyNoRestoreData(app_id2);

  // Modify `app_id1` status to be ready to simulate `app_id1` is installed.
  UpdateApp(app_id1, apps::Readiness::kReady);
  EXPECT_FALSE(HasRestoreData(app_id1));
  EXPECT_TRUE(GetAppIds().empty());
  VerifyNoRestoreData(app_id1);

  arc_helper_.StopInstance();
}

// Verify the saved windows in ArcAppLaunchHandler when apps status are
// modified.
IN_PROC_BROWSER_TEST_F(ArcAppQueueRestoreHandlerArcAppBrowserTest, UpdateApps) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);

  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  // Simulate creating kTaskId2 for the app2.
  int32_t kTaskId2 = 101;
  arc_helper_.CreateTask(app_id2, kTaskId2, session_id2);

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kNormal);

  WaitForAppLaunchInfoSaved();

  // Modify apps status before restoring, so that apps can't be restored.
  UpdateApp(app_id1, apps::Readiness::kDisabledByPolicy);
  UpdateApp(app_id2, apps::Readiness::kDisabledByPolicy);
  base::HistogramTester histogram_tester;
  Restore();
  widget1->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);
  app_host()->OnTaskDestroyed(kTaskId2);

  std::set<std::string> app_ids = GetAppIds();
  EXPECT_EQ(2u, app_ids.size());
  EXPECT_TRUE(base::Contains(app_ids, app_id1));
  EXPECT_TRUE(base::Contains(app_ids, app_id2));
  EXPECT_FALSE(HasRestoreData());

  // Modify `app_id1` status to be ready to prepare launching `app_id1`.
  UpdateApp(app_id1, apps::Readiness::kReady);
  app_ids = GetAppIds();
  EXPECT_FALSE(base::Contains(app_ids, app_id1));
  EXPECT_TRUE(base::Contains(app_ids, app_id2));
  VerifyWindows(activation_index1, app_id1, kTaskId1);

  // Modify `app_id2` status to be ready to prepare launching `app_id2`.
  UpdateApp(app_id2, apps::Readiness::kReady);
  app_ids = GetAppIds();
  EXPECT_FALSE(base::Contains(app_ids, app_id1));
  EXPECT_FALSE(base::Contains(app_ids, app_id2));
  VerifyNoStackWindows(app_id2, kTaskId2);

  // Verify the restore data and windows for `app_id1` and `app_id2` are not
  // removed.
  VerifyRestoreData(app_id1, kTaskId1);
  VerifyRestoreData(app_id2, kTaskId2);

  OnAppConnectionReady();
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   app_restore::kRestoredAppWindowCountHistogram, 2));

  arc_helper_.StopInstance();
}

// Verify the saved windows in ArcAppLaunchHandler when only restore one of the
// apps.
IN_PROC_BROWSER_TEST_F(ArcAppQueueRestoreHandlerArcAppBrowserTest,
                       RestoreOneApp) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);

  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();

  SaveAppLaunchInfo(app_id1, session_id1);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kNormal);

  WaitForAppLaunchInfoSaved();

  base::HistogramTester histogram_tester;
  Restore();
  widget1->CloseNow();
  app_host()->OnTaskDestroyed(kTaskId1);

  EXPECT_TRUE(GetAppIds().empty());
  EXPECT_TRUE(HasRestoreData());
  VerifyWindows(activation_index1, app_id1, kTaskId1);

  // Verify the restore data and windows for `app_id1` are not removed.
  VerifyRestoreData(app_id1, kTaskId1);
  VerifyNoRestoreData(app_id2);

  OnAppConnectionReady();
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   app_restore::kRestoredAppWindowCountHistogram, 1));

  arc_helper_.StopInstance();
}

// Verify the saved windows in ArcAppLaunchHandler when one of apps is ready
// late.
IN_PROC_BROWSER_TEST_F(ArcAppQueueRestoreHandlerArcAppBrowserTest,
                       AppIsReadyLate) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);

  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  // Simulate creating kTaskId2 for the app2.
  int32_t kTaskId2 = 101;
  arc_helper_.CreateTask(app_id2, kTaskId2, session_id2);

  WaitForAppLaunchInfoSaved();

  int32_t activation_index1 = 11;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kNormal);

  WaitForAppLaunchInfoSaved();

  // Remove `app_id2` before restoring, so that `app_id2` can't be restored.
  RemoveApp(app_id2);
  base::HistogramTester histogram_tester;
  Restore();
  widget1->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);
  app_host()->OnTaskDestroyed(kTaskId2);

  std::set<std::string> app_ids = GetAppIds();
  EXPECT_EQ(1u, app_ids.size());
  EXPECT_TRUE(base::Contains(app_ids, app_id2));
  EXPECT_TRUE(HasRestoreData());
  VerifyWindows(activation_index1, app_id1, kTaskId1);

  OnAppConnectionReady();
  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   app_restore::kRestoredAppWindowCountHistogram, 1));

  // Modify `app_id2` status to be ready to prepare launching `app_id2`.
  UpdateApp(app_id2, apps::Readiness::kReady);
  EXPECT_TRUE(GetAppIds().empty());
  EXPECT_TRUE(HasRestoreData());
  VerifyWindows(activation_index1, app_id1, kTaskId1);
  VerifyNoStackWindows(app_id2, kTaskId2);

  // Verify the restore data and windows for `app_id1` and `app_id2` are not
  // removed.
  VerifyRestoreData(app_id1, kTaskId1);
  VerifyRestoreData(app_id2, kTaskId2);

  arc_helper_.StopInstance();
}

// Verify the restore process when the user clicks the `restore` button very
// late after the OnAppConnectionReady is called.
IN_PROC_BROWSER_TEST_F(ArcAppQueueRestoreHandlerArcAppBrowserTest,
                       RestoreLate) {
  SetProfile();
  arc_helper_.InstallTestApps(kTestAppPackage, true);

  const std::string app_id1 = GetTestApp1Id(kTestAppPackage);
  const std::string app_id2 = GetTestApp2Id(kTestAppPackage);

  int32_t session_id1 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();
  int32_t session_id2 =
      ::full_restore::FullRestoreSaveHandler::GetInstance()->GetArcSessionId();

  SaveAppLaunchInfo(app_id1, session_id1);
  SaveAppLaunchInfo(app_id2, session_id2);

  // Simulate creating kTaskId1. The task id needs to match the |window_app_id|
  // arg of CreateExoWindow.
  int32_t kTaskId1 = 100;
  arc_helper_.CreateTask(app_id1, kTaskId1, session_id1);

  // Create the window for the app1.
  views::Widget* widget1 = CreateExoWindow("org.chromium.arc.100");
  aura::Window* window1 = widget1->GetNativeWindow();

  // Simulate creating kTaskId2 for the app2.
  int32_t kTaskId2 = 101;
  arc_helper_.CreateTask(app_id2, kTaskId2, session_id2);

  int32_t activation_index1 = 11;
  SaveWindowInfo(window1, activation_index1,
                 chromeos::WindowStateType::kNormal);

  WaitForAppLaunchInfoSaved();

  // Call OnAppConnectionReady to simulate the app connection is ready.
  base::HistogramTester histogram_tester;
  OnAppConnectionReady();

  // Simulate the user clicks the `restore` button.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());
  SetShouldRestore(app_launch_handler.get());

  widget1->CloseNow();

  app_host()->OnTaskDestroyed(kTaskId1);
  app_host()->OnTaskDestroyed(kTaskId2);

  EXPECT_TRUE(HasRestoreData());
  VerifyWindows(activation_index1, app_id1, kTaskId1);
  VerifyNoStackWindows(app_id2, kTaskId2);

  EXPECT_EQ(1, histogram_tester.GetBucketCount(
                   app_restore::kRestoredAppWindowCountHistogram, 2));

  arc_helper_.StopInstance();
}

class FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest
    : public SystemWebAppIntegrationTest {
 public:
  FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest() {
    OsUrlHandlerSystemWebAppDelegate::EnableDelegateForTesting(true);
  }

  ~FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest() override {
    OsUrlHandlerSystemWebAppDelegate::EnableDelegateForTesting(false);
  }

  Browser* LaunchSystemWebApp(const GURL& gurl,
                              SystemWebAppType system_app_type,
                              apps::LaunchSource launch_source =
                                  apps::LaunchSource::kFromChromeInternal) {
    WaitForTestSystemAppInstall();

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    content::TestNavigationObserver navigation_observer(gurl);
    navigation_observer.StartWatchingNewWebContents();

    proxy->Launch(
        *GetManager().GetAppIdForSystemApp(system_app_type), ui::EF_NONE,
        launch_source,
        std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));

    navigation_observer.Wait();

    return BrowserList::GetInstance()->GetLastActive();
  }

  Browser* LaunchSystemWebAppWithOverrideURL(SystemWebAppType system_app_type,
                                             const GURL& override_url) {
    WaitForTestSystemAppInstall();

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    content::TestNavigationObserver navigation_observer(override_url);
    navigation_observer.StartWatchingNewWebContents();

    proxy->LaunchAppWithUrl(
        *GetManager().GetAppIdForSystemApp(system_app_type), ui::EF_NONE,
        override_url, apps::LaunchSource::kFromChromeInternal,
        std::make_unique<apps::WindowInfo>(display::kDefaultDisplayId));

    navigation_observer.Wait();

    return BrowserList::GetInstance()->GetLastActive();
  }

  Browser* LaunchSystemWebApp(apps::LaunchSource launch_source =
                                  apps::LaunchSource::kFromChromeInternal) {
    return LaunchSystemWebApp(GURL("chrome://help-app/"),
                              SystemWebAppType::HELP, launch_source);
  }

  // Launches the media system web app. Used when a test needs to use a
  // different system web app.
  Browser* LaunchMediaSystemWebApp(
      apps::LaunchSource launch_source =
          apps::LaunchSource::kFromChromeInternal) {
    return LaunchSystemWebApp(GURL("chrome://media-app/"),
                              SystemWebAppType::MEDIA, launch_source);
  }

  void WaitForAppLaunchInfoSaved(bool allow_save = true) {
    ::full_restore::FullRestoreSaveHandler* save_handler =
        ::full_restore::FullRestoreSaveHandler::GetInstance();

    if (allow_save)
      save_handler->AllowSave();

    base::OneShotTimer* timer = save_handler->GetTimerForTesting();
    if (timer->IsRunning()) {
      // Simulate timeout, and the launch info is saved.
      timer->FireNow();
    }
    content::RunAllTasksUntilIdle();

    ::full_restore::FullRestoreReadHandler::GetInstance()
        ->profile_path_to_restore_data_.clear();
  }

  void ModifyAppReadiness(apps::Readiness readiness) {
    apps::AppType app_type = apps::AppType::kWeb;
    if (crosapi::browser_util::IsLacrosEnabled() &&
        web_app::IsWebAppsCrosapiEnabled()) {
      app_type = apps::AppType::kSystemWeb;
    }

    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    apps::AppPtr app = std::make_unique<apps::App>(
        app_type, *GetManager().GetAppIdForSystemApp(SystemWebAppType::HELP));
    app->readiness = readiness;
    std::vector<apps::AppPtr> deltas;
    deltas.push_back(std::move(app));
    cache.OnApps(std::move(deltas), app_type,
                 false /* should_notify_initialized */);
  }

  void SetShouldRestore(FullRestoreAppLaunchHandler* app_launch_handler) {
    content::RunAllTasksUntilIdle();
    app_launch_handler->SetShouldRestore();
    content::RunAllTasksUntilIdle();
  }

  bool HasWindowInfo(int32_t restore_window_id) {
    return ::full_restore::FullRestoreReadHandler::GetInstance()->HasWindowInfo(
        restore_window_id);
  }
};

IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       LaunchSWA) {
  Browser* app_browser = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  ASSERT_FALSE(HasWindowInfo(window_id));

  SetShouldRestore(app_launch_handler.get());

  ASSERT_TRUE(HasWindowInfo(window_id));

  // Get the restored browser for the system web app.
  Browser* restore_app_browser = GetBrowserForWindowId(window_id);
  ASSERT_TRUE(restore_app_browser);
  ASSERT_NE(browser(), restore_app_browser);

  // Get the restore window id.
  window = restore_app_browser->window()->GetNativeWindow();
  int32_t restore_window_id =
      window->GetProperty(::app_restore::kRestoreWindowIdKey);

  EXPECT_EQ(window_id, restore_window_id);
}

// Ensure that Full Restore respects the override URL specified in a SWA's
// AppLaunchParams if configured to do so.
IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       LaunchSWAWithRestoreOverrideURL) {
  const auto swa_type = SystemWebAppType::OS_URL_HANDLER;
  const auto override_url = GURL(chrome::kChromeUIVersionURL);

  Browser* app_browser =
      LaunchSystemWebAppWithOverrideURL(swa_type, override_url);
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  ASSERT_FALSE(HasWindowInfo(window_id));

  content::TestNavigationObserver navigation_observer(override_url);
  navigation_observer.StartWatchingNewWebContents();
  SetShouldRestore(app_launch_handler.get());
  navigation_observer.Wait();

  ASSERT_TRUE(HasWindowInfo(window_id));

  // Get the restored browser for the system web app.
  Browser* restore_app_browser = GetBrowserForWindowId(window_id);
  ASSERT_TRUE(restore_app_browser);
  ASSERT_NE(browser(), restore_app_browser);

  // Get the restore window id.
  window = restore_app_browser->window()->GetNativeWindow();
  int32_t restore_window_id =
      window->GetProperty(::app_restore::kRestoreWindowIdKey);

  EXPECT_EQ(window_id, restore_window_id);

  EXPECT_EQ(override_url, restore_app_browser->tab_strip_model()
                              ->GetActiveWebContents()
                              ->GetLastCommittedURL());
}

// Verify that when the full restore doesn't start, the browser window of the
// SWA doesn't have the restore info.
IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       LaunchSWAWithoutRestore) {
  Browser* app_browser = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::app_restore::kWindowIdKey);

  SaveWindowInfo(window);
  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  content::RunAllTasksUntilIdle();

  ASSERT_FALSE(HasWindowInfo(window_id));

  Browser* new_app_browser = LaunchSystemWebApp();

  ASSERT_TRUE(new_app_browser);
  ASSERT_NE(browser(), new_app_browser);

  window = new_app_browser->window()->GetNativeWindow();
  auto* window_state = WindowState::Get(window);
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verify the restoration if the SWA is not available when set
// restore, and the restoration can work if the SWA is added later.
IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       NoSWAWhenRestore) {
  Browser* app_browser = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved();

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  // Modify the app readiness to uninstall to simulate the app is not installed
  // during the system startup phase.
  ModifyAppReadiness(apps::Readiness::kUninstalledByUser);

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  SetShouldRestore(app_launch_handler.get());

  // Verify the app is not restored because the app is not installed.
  Browser* restore_app_browser = GetBrowserForWindowId(window_id);
  ASSERT_FALSE(restore_app_browser);

  // Modify the app readiness to kReady to simulate the app is installed.
  ModifyAppReadiness(apps::Readiness::kReady);

  // Wait for the restoration.
  content::RunAllTasksUntilIdle();

  // Get the restored browser for the system web app to verify the app is
  // restored.
  restore_app_browser = GetBrowserForWindowId(window_id);
  ASSERT_TRUE(restore_app_browser);
  ASSERT_NE(browser(), restore_app_browser);

  // Get the restore window id.
  window = restore_app_browser->window()->GetNativeWindow();
  int32_t restore_window_id =
      window->GetProperty(::app_restore::kRestoreWindowIdKey);

  EXPECT_EQ(window_id, restore_window_id);
}

// Launch the help app. Reboot, no restore. Launch the media app by the system.
// Reboot, verify the help app can be restored.
IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       RestartMutiTimesWithLaunchBySystem) {
  Browser* app_browser1 = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser1);
  ASSERT_NE(browser(), app_browser1);

  // Get the window id.
  aura::Window* window1 = app_browser1->window()->GetNativeWindow();
  int32_t window_id1 = window1->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved();
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser1);

  // Modify the app readiness to uninstall to simulate the app is not installed
  // during the system startup phase.
  ModifyAppReadiness(apps::Readiness::kUninstalledByUser);

  // Create FullRestoreAppLaunchHandler to simulate the system reboot.
  auto app_launch_handler1 =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  content::RunAllTasksUntilIdle();

  // Verify the app is not restored because the app is not installed.
  ASSERT_FALSE(GetBrowserForWindowId(window_id1));

  // Launch another app from kFromChromeInternal, so not start the save timer,
  // to prevent overwriting the full restore file.
  Browser* app_browser2 = LaunchMediaSystemWebApp();
  ASSERT_TRUE(app_browser2);
  aura::Window* window2 = app_browser2->window()->GetNativeWindow();
  int32_t window_id2 = window2->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved(/*allow_save=*/false);
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();
  app_launch_handler1.reset();

  // Modify the app readiness to kReady to simulate the app is installed.
  ModifyAppReadiness(apps::Readiness::kReady);

  // Create FullRestoreAppLaunchHandler to simulate the system reboot again.
  auto app_launch_handler2 =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  content::RunAllTasksUntilIdle();

  SetShouldRestore(app_launch_handler2.get());

  // Wait for the restoration.
  content::RunAllTasksUntilIdle();

  ASSERT_FALSE(GetBrowserForWindowId(window_id2));

  // Get the restored browser for the system web app to verify the app is
  // restored.
  auto* restore_app_browser = GetBrowserForWindowId(window_id1);
  ASSERT_TRUE(restore_app_browser);

  // Get the restore window id.
  window1 = restore_app_browser->window()->GetNativeWindow();
  int32_t restore_window_id =
      window1->GetProperty(::app_restore::kRestoreWindowIdKey);

  EXPECT_EQ(window_id1, restore_window_id);
}

// Launch the help app. Reboot, no restore. Launch the media app by the user.
// Reboot, verify the media app can be restored.
IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       RestartMutiTimesWithLaunchByUser) {
  Browser* app_browser1 = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser1);
  ASSERT_NE(browser(), app_browser1);

  // Get the window id.
  aura::Window* window1 = app_browser1->window()->GetNativeWindow();
  int32_t window_id1 = window1->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved();
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  // Close app_browser so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser1);

  // Modify the app readiness to uninstall to simulate the app is not installed
  // during the system startup phase.
  ModifyAppReadiness(apps::Readiness::kUninstalledByUser);

  // Create FullRestoreAppLaunchHandler to simulate the system reboot.
  auto app_launch_handler1 =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  content::RunAllTasksUntilIdle();

  // Verify the app is not restored because the app is not installed.
  ASSERT_FALSE(GetBrowserForWindowId(window_id1));

  // Launch another app from kFromShelf, so start the save timer,
  Browser* app_browser2 =
      LaunchMediaSystemWebApp(apps::LaunchSource::kFromShelf);
  ASSERT_TRUE(app_browser2);
  aura::Window* window2 = app_browser2->window()->GetNativeWindow();
  int32_t window_id2 = window2->GetProperty(::app_restore::kWindowIdKey);

  WaitForAppLaunchInfoSaved(/*allow_save=*/false);
  ::full_restore::FullRestoreSaveHandler::GetInstance()->ClearForTesting();

  web_app::CloseAndWait(app_browser2);

  app_launch_handler1.reset();

  // Modify the app readiness to kReady to simulate the app is installed.
  ModifyAppReadiness(apps::Readiness::kReady);

  // Create FullRestoreAppLaunchHandler to simulate the system reboot again.
  auto app_launch_handler2 =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  content::RunAllTasksUntilIdle();

  SetShouldRestore(app_launch_handler2.get());

  // Wait for the restoration.
  content::RunAllTasksUntilIdle();

  ASSERT_FALSE(GetBrowserForWindowId(window_id1));

  // Get the restored browser for the system web app to verify the app is
  // restored.
  auto* restore_app_browser = GetBrowserForWindowId(window_id2);
  ASSERT_TRUE(restore_app_browser);

  // Get the restore window id.
  window2 = restore_app_browser->window()->GetNativeWindow();
  int32_t restore_window_id =
      window2->GetProperty(::app_restore::kRestoreWindowIdKey);

  EXPECT_EQ(window_id2, restore_window_id);
}

IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       WindowProperties) {
  Browser* app_browser = LaunchSystemWebApp();
  ASSERT_TRUE(app_browser);
  ASSERT_NE(browser(), app_browser);

  // Get the window id.
  aura::Window* window = app_browser->window()->GetNativeWindow();
  int32_t window_id = window->GetProperty(::app_restore::kWindowIdKey);

  // Snap |window| to the left and store its window properties.
  // TODO(sammiequon): Store and check desk id and restore bounds.
  auto* window_state = WindowState::Get(window);
  const WindowSnapWMEvent left_snap_event(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&left_snap_event);
  const chromeos::WindowStateType pre_save_state_type =
      window_state->GetStateType();
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped, pre_save_state_type);
  const gfx::Rect pre_save_bounds = window->GetBoundsInScreen();

  SaveWindowInfo(window);
  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  // Close |app_browser| so that the SWA can be relaunched.
  web_app::CloseAndWait(app_browser);

  SetShouldRestore(app_launch_handler.get());

  // Get the restored browser for the system web app.
  Browser* restore_app_browser = GetBrowserForWindowId(window_id);
  ASSERT_TRUE(restore_app_browser);
  ASSERT_NE(browser(), restore_app_browser);

  // Get the restored browser's window.
  window = restore_app_browser->window()->GetNativeWindow();
  ASSERT_EQ(window_id, window->GetProperty(::app_restore::kRestoreWindowIdKey));

  // Check that |window|'s properties match the one's we stored.
  EXPECT_EQ(pre_save_bounds, window->GetBoundsInScreen());
  window_state = WindowState::Get(window);
  EXPECT_EQ(pre_save_state_type, window_state->GetStateType());

  // Verify that |window_state| has viable restore bounds for when the user
  // wants to return to normal window show state. Regression test for
  // https://crbug.com/1188986.
  EXPECT_TRUE(window_state->HasRestoreBounds());
}

// Tests that apps maintain splitview snap status after being relaunched with
// full restore.
IN_PROC_BROWSER_TEST_P(FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest,
                       TabletSplitView) {
  TabletMode::Get()->SetEnabledForTest(true);

  Browser* app1_browser = LaunchSystemWebApp();
  Browser* app2_browser = LaunchMediaSystemWebApp();

  aura::Window* app1_window = app1_browser->window()->GetNativeWindow();
  aura::Window* app2_window = app2_browser->window()->GetNativeWindow();

  SplitViewTestApi split_view_test_api;
  split_view_test_api.SnapWindow(app1_window,
                                 SplitViewTestApi::SnapPosition::LEFT);
  split_view_test_api.SnapWindow(app2_window,
                                 SplitViewTestApi::SnapPosition::RIGHT);
  ASSERT_EQ(app1_window, split_view_test_api.GetLeftWindow());
  ASSERT_EQ(app2_window, split_view_test_api.GetRightWindow());

  const int32_t app1_id = app1_window->GetProperty(::app_restore::kWindowIdKey);
  const int32_t app2_id = app2_window->GetProperty(::app_restore::kWindowIdKey);

  SaveWindowInfo(app1_window);
  SaveWindowInfo(app2_window);
  WaitForAppLaunchInfoSaved();

  // Create FullRestoreAppLaunchHandler.
  auto app_launch_handler =
      std::make_unique<FullRestoreAppLaunchHandler>(profile());

  // Close `app1_browser` and `app2_browser` so that the SWA can be relaunched.
  web_app::CloseAndWait(app1_browser);
  web_app::CloseAndWait(app2_browser);

  SetShouldRestore(app_launch_handler.get());

  aura::Window* restore_app1_window = nullptr;
  aura::Window* restore_app2_window = nullptr;

  // Find the restored app windows in the browser list.
  for (Browser* browser : *BrowserList::GetInstance()) {
    aura::Window* native_window = browser->window()->GetNativeWindow();
    if (native_window->GetProperty(::app_restore::kRestoreWindowIdKey) ==
        app1_id) {
      restore_app1_window = native_window;
    }
    if (native_window->GetProperty(::app_restore::kRestoreWindowIdKey) ==
        app2_id) {
      restore_app2_window = native_window;
    }
  }

  ASSERT_TRUE(restore_app1_window);
  ASSERT_TRUE(restore_app2_window);
  EXPECT_EQ(restore_app1_window, split_view_test_api.GetLeftWindow());
  EXPECT_EQ(restore_app2_window, split_view_test_api.GetRightWindow());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    FullRestoreAppLaunchHandlerSystemWebAppsBrowserTest);

}  // namespace ash::full_restore
