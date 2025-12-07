// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_test_api.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_test_util.h"
#include "base/check_deref.h"
#include "base/test/gtest_tags.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/test/fake_cws_chrome_apps.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/app_mode/test/network_state_mixin.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/test/test_browser_closed_waiter.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/app_window/app_window.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"

namespace ash {

using kiosk::test::AutoLaunchKioskApp;
using kiosk::test::CloseAppWindow;
using kiosk::test::CurrentProfile;
using kiosk::test::IsAppInstalled;
using kiosk::test::LaunchAppManually;
using kiosk::test::OfflineEnabledChromeAppV1;
using kiosk::test::OpenA11ySettings;
using kiosk::test::TheKioskApp;
using kiosk::test::WaitKioskLaunched;

namespace {

void AddKioskLaunchTagIdToTestResult(KioskAppType app_type) {
  // Workflow COM_KIOSK_CUJ3_TASK4_WF1.
  constexpr char kLaunchChromeAppTag[] =
      "screenplay-5e6b8c54-2eab-4ac0-a484-b9738466bb9b";
  // Workflow COM_KIOSK_CUJ3_TASK5_WF1.
  constexpr char kLaunchWebAppTag[] =
      "screenplay-d93db63f-372e-4c3b-a1ca-3f23587dbac4";

  switch (app_type) {
    case KioskAppType::kChromeApp:
      base::AddFeatureIdTagToTestResult(kLaunchChromeAppTag);
      break;
    case KioskAppType::kWebApp:
      base::AddFeatureIdTagToTestResult(kLaunchWebAppTag);
      break;
    case KioskAppType::kIsolatedWebApp:
    case KioskAppType::kArcvmApp:
      break;
  }
}

void SimulateSwipeUpGesture() {
  auto& root_window = CHECK_DEREF(Shell::Get()->GetPrimaryRootWindow());
  auto display_bounds = root_window.bounds();
  const gfx::Point start_point(
      display_bounds.width() / 4,
      display_bounds.bottom() - ShelfConfig::Get()->shelf_size() / 2);
  gfx::Point end_point(start_point.x(), start_point.y() - 80);

  ui::test::EventGenerator(&root_window)
      .GestureScrollSequence(start_point, end_point,
                             /*duration=*/base::Milliseconds(300), /*steps=*/4);
}



}  // namespace

// Verifies generic Kiosk behavior.
class KioskTest : public MixinBasedInProcessBrowserTest,
                  public testing::WithParamInterface<KioskMixin::Config> {
 public:
  KioskTest() = default;

  KioskTest(const KioskTest&) = delete;
  KioskTest& operator=(const KioskTest&) = delete;

  ~KioskTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(KioskTest, LaunchesAndInstallsApp) {
  AddKioskLaunchTagIdToTestResult(AutoLaunchKioskApp().id().type);
  ASSERT_TRUE(IsAppInstalled(CurrentProfile(), AutoLaunchKioskApp()));
}

IN_PROC_BROWSER_TEST_P(KioskTest, HidesShelf) {
  EXPECT_FALSE(ShelfTestApi().IsVisible());

  SimulateSwipeUpGesture();
  EXPECT_FALSE(ShelfTestApi().IsVisible());
}

IN_PROC_BROWSER_TEST_P(KioskTest, CanOpenA11ySettings) {
  Browser* settings = OpenA11ySettings(
      CHECK_DEREF(user_manager::UserManager::Get()->GetActiveUser()));
  ASSERT_NE(settings, nullptr);
  EXPECT_TRUE(settings->window()->IsActive());
  EXPECT_TRUE(settings->window()->IsVisible());
}

IN_PROC_BROWSER_TEST_P(KioskTest, ExitsIfOnlySettingsWindowRemainsOpen) {
  Browser& settings = CHECK_DEREF(OpenA11ySettings(
      CHECK_DEREF(user_manager::UserManager::Get()->GetActiveUser())));
  EXPECT_GT(chrome::GetTotalBrowserCount(), 0u);

  // Close the app window and verify the settings browser gets closed too.
  CloseAppWindow(AutoLaunchKioskApp());
  ASSERT_TRUE(TestBrowserClosedWaiter(&settings).WaitUntilClosed());
  EXPECT_EQ(chrome::GetTotalBrowserCount(), 0u);

  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
  EXPECT_TRUE(session.is_shutting_down());
}

IN_PROC_BROWSER_TEST_P(KioskTest, DoesNotExitWhenSettingsWindowCloses) {
  Browser& settings = CHECK_DEREF(OpenA11ySettings(
      CHECK_DEREF(user_manager::UserManager::Get()->GetActiveUser())));
  EXPECT_EQ(GetLastActiveBrowserWindowInterfaceWithAnyProfile(), &settings);

  settings.window()->Close();
  ASSERT_TRUE(TestBrowserClosedWaiter(&settings).WaitUntilClosed());
  auto& session = CHECK_DEREF(KioskController::Get().GetKioskSystemSession());
  EXPECT_FALSE(session.is_shutting_down());
}

IN_PROC_BROWSER_TEST_P(KioskTest, DoesNotSignInWithGaiaAccount) {
  const auto& manager =
      CHECK_DEREF(IdentityManagerFactory::GetForProfile(&CurrentProfile()));
  EXPECT_FALSE(manager.HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_P(KioskTest, CannotCreateNewDesksDuringKioskSession) {
  int initial_desks_count = DesksController::Get()->GetNumberOfDesks();
  EXPECT_EQ(1, initial_desks_count);
  ASSERT_FALSE(DesksController::Get()->CanCreateDesks());
}

IN_PROC_BROWSER_TEST_P(KioskTest, CannotEnterOverviewDuringKioskSession) {
  auto* overview_controller = Shell::Get()->overview_controller();

  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);

  ASSERT_FALSE(overview_controller->InOverviewSession());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    KioskTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

// Verifies generic online/offline related behavior in Kiosk.
class OfflineKioskTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  OfflineKioskTest() = default;

  OfflineKioskTest(const OfflineKioskTest&) = delete;
  OfflineKioskTest& operator=(const OfflineKioskTest&) = delete;

  ~OfflineKioskTest() override = default;

  static std::vector<KioskMixin::Config> Configs() {
    // TODO(crbug.com/379633748): Add IWA.
    return {
        KioskMixin::Config{/*name=*/"WebApp",
                           /*auto_launch_account_id=*/{},
                           {KioskMixin::SimpleWebAppOption()}},
        KioskMixin::Config{/*name=*/"ChromeApp",
                           /*auto_launch_account_id=*/{},
                           // The Chrome app needs to be offline enabled because
                           // some tests will launch it while offline.
                           {OfflineEnabledChromeAppV1()}}};
  }

  NetworkStateMixin network_state_{&mixin_host_};

  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
};

IN_PROC_BROWSER_TEST_P(OfflineKioskTest, OfflineLaunchWorksOnceItComesOnline) {
  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));

  network_state_.SimulateOnline();
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(OfflineKioskTest, PRE_LaunchesInstalledAppOffline) {
  network_state_.SimulateOnline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
  ASSERT_TRUE((IsAppInstalled(CurrentProfile(), TheKioskApp())));
}

IN_PROC_BROWSER_TEST_P(OfflineKioskTest, LaunchesInstalledAppOffline) {
  network_state_.SimulateOffline();
  ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
  ASSERT_TRUE(WaitKioskLaunched());
}

INSTANTIATE_TEST_SUITE_P(All,
                         OfflineKioskTest,
                         testing::ValuesIn(OfflineKioskTest::Configs()),
                         KioskMixin::ConfigName);

}  // namespace ash
