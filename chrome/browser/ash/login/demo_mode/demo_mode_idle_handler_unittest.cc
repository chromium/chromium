// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_mode_idle_handler.h"

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/metrics/demo_session_metrics_recorder.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/files/scoped_temp_dir.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_window_closer.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/test_browser_window.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/demo_mode/utils/demo_session_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {
namespace {

const base::TimeDelta kReLuanchDemoAppIdleDuration = base::Seconds(90);
const base::TimeDelta kLogoutDelayMax = base::Minutes(90);

const char kUser[] = "user@gmail.com";
const AccountId kAccountId =
    AccountId::FromUserEmailGaiaId(kUser, GaiaId("1111"));
constexpr SkColor kWallpaperColor = SK_ColorMAGENTA;

}  // namespace

class DemoModeIdleHandlerTestBase : public ChromeAshTestBase {
 protected:
  DemoModeIdleHandlerTestBase()
      : ChromeAshTestBase(std::make_unique<content::BrowserTaskEnvironment>(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME)),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {
    window_closer_ = std::make_unique<DemoModeWindowCloser>(
        base::BindRepeating(&DemoModeIdleHandlerTestBase::MockLaunchDemoModeApp,
                            base::Unretained(this)));
  }
  ~DemoModeIdleHandlerTestBase() override = default;

  void SetUp() override {
    ChromeAshTestBase::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kUser);
    fake_user_manager_->AddUser(kAccountId);
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    wallpaper_controller_ = Shell::Get()->wallpaper_controller();
    wallpaper_controller_->Init(
        base::FilePath(), /*online_wallpaper_dir=*/base::FilePath(),
        /* custom_wallpaper_dir=*/user_data_dir_.GetPath(),
        /* policy_wallpaper=*/base::FilePath());

    wallpaper_controller_->SetClient(&client_);
    client_.set_fake_files_id_for_account_id(kAccountId, "wallpaper_files_id");
    client_.set_wallpaper_sync_enabled(false);
    wallpaper_controller_->set_bypass_decode_for_testing();

    fake_user_manager_->LoginUser(kAccountId);

    // We need to create `metrics_recorder_` in idle handler unit test here
    // because `DemoModeIdleHandler::OnIdle()` will
    // `ReportShopperSessionDwellTime()`, which requires `first_user_activity_`
    // to be not null. Once `metrics_recorder_` is created, it'll observe user
    // activity to set `first_user_activity_`.
    metrics_recorder_ = std::make_unique<DemoSessionMetricsRecorder>();

    // OK to unretained `this` since the life cycle of `demo_mode_idle_handler_`
    // is the same as the tests.
    demo_mode_idle_handler_ = std::make_unique<DemoModeIdleHandler>(
        window_closer_.get(),
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }

  void TearDown() override {
    // metrics_recorder_ needs to be destroyed first because it still needs some
    // services to report some metrics.
    metrics_recorder_.reset();
    ChromeAshTestBase::TearDown();
    profile_ = nullptr;
    profile_manager_.DeleteAllTestingProfiles();
    demo_mode_idle_handler_.reset();
    window_closer_.reset();
  }

  void SimulateUserActivity() {
    ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  }

  void FastForwardBy(base::TimeDelta time) {
    task_environment()->FastForwardBy(time);
  }

  void MockLaunchDemoModeApp() { launch_demo_app_count_++; }

  int get_launch_demo_app_count() { return launch_demo_app_count_; }
  Profile* profile() { return profile_; }

  WallpaperControllerImpl* wallpaper_controller() {
    return wallpaper_controller_;
  }

  DemoModeIdleHandler* demo_mode_idle_handler() {
    return demo_mode_idle_handler_.get();
  }

 private:
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_{std::make_unique<FakeChromeUserManager>()};
  int launch_demo_app_count_ = 0;
  TestingProfileManager profile_manager_;

  std::unique_ptr<DemoModeWindowCloser> window_closer_;
  std::unique_ptr<DemoModeIdleHandler> demo_mode_idle_handler_;
  raw_ptr<Profile> profile_ = nullptr;

  TestWallpaperControllerClient client_;
  // Disable the dangling detection since we don't own `wallpaper_controller_`.
  raw_ptr<WallpaperControllerImpl, DisableDanglingPtrDetection>
      wallpaper_controller_ = nullptr;
  base::ScopedTempDir user_data_dir_;
  std::unique_ptr<DemoSessionMetricsRecorder> metrics_recorder_;
};

// DemoIdleHandler test for shopper session.
class DemoModeIdleHandlerTest : public DemoModeIdleHandlerTestBase {
  void SetUp() override {
    DemoModeIdleHandlerTestBase::SetUp();
    demo_mode::SetDoNothingWhenPowerIdle();
  }
};

TEST_F(DemoModeIdleHandlerTest, CloseAllBrowsers) {
  // Ensure MGS logout timer not started.
  EXPECT_FALSE(
      demo_mode_idle_handler()->GetMGSLogoutTimeoutForTest().has_value());

  // Initialize 2 browsers.
  std::unique_ptr<Browser> browser_1 = CreateBrowserWithTestWindowForParams(
      Browser::CreateParams(profile(), /*user_gesture=*/true));
  std::unique_ptr<Browser> browser_2 = CreateBrowserWithTestWindowForParams(
      Browser::CreateParams(profile(), /*user_gesture=*/true));
  EXPECT_EQ(BrowserList::GetInstance()->size(), 2U);

  // Trigger close all browsers by being idle for
  // `kReLuanchDemoAppIdleDuration`.
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  EXPECT_TRUE(static_cast<TestBrowserWindow*>(browser_1->window())->IsClosed());
  EXPECT_TRUE(static_cast<TestBrowserWindow*>(browser_2->window())->IsClosed());
  // `TestBrowserWindow` does not destroy `Browser` when `Close()` is called,
  // but real browser window does. Reset both browsers here to fake this
  // behavior.
  browser_1.reset();
  browser_2.reset();

  EXPECT_EQ(get_launch_demo_app_count(), 1);
  EXPECT_TRUE(BrowserList::GetInstance()->empty());
}

TEST_F(DemoModeIdleHandlerTest, ClearAndCloseClipboard) {
  ash::ClipboardHistoryControllerImpl* clipboard_history_controller =
      ash::Shell::Get()->clipboard_history_controller();
  base::test::TestFuture<bool> operation_confirmed_future;
  clipboard_history_controller->set_confirmed_operation_callback_for_test(
      operation_confirmed_future.GetRepeatingCallback());

  // Write text to the clipboard
  {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(u"test");
  }
  // Wait for the operation to be confirmed.
  EXPECT_TRUE(operation_confirmed_future.Take());

  // Make sure the clipboard is not empty.
  const std::list<ClipboardHistoryItem>& items =
      clipboard_history_controller->history()->GetItems();
  EXPECT_TRUE(!items.empty());

  // Show the clipboard menu.
  PressAndReleaseKey(ui::VKEY_V, ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(clipboard_history_controller->IsMenuShowing());

  // Trigger clearing clipboard by being idle for `kReLuanchDemoAppIdleDuration`
  // period of time.
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration);

  // Expect the clipboard to be not showing.
  EXPECT_FALSE(clipboard_history_controller->IsMenuShowing());
  // And nothing in the clipboard.
  const std::list<ClipboardHistoryItem>& no_items =
      clipboard_history_controller->history()->GetItems();
  EXPECT_TRUE(no_items.empty());
}

TEST_F(DemoModeIdleHandlerTest, ResetWallpaper) {
  // Set a custom wallpaper at first.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(640, 480);
  bitmap.eraseColor(kWallpaperColor);
  base::RunLoop loop;
  wallpaper_controller()->SetDecodedCustomWallpaper(
      kAccountId, "file_name", WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, base::BindLambdaForTesting([&loop](bool success) {
        EXPECT_TRUE(success);
        loop.Quit();
      }),
      /*file_path=*/"", gfx::ImageSkia::CreateFrom1xBitmap(bitmap));
  loop.Run();
  // Expect not a default wallpaper.
  EXPECT_NE(wallpaper_controller()->GetWallpaperType(),
            WallpaperType::kDefault);

  // Expect wallpaper reset to default on idle.
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  EXPECT_EQ(wallpaper_controller()->GetWallpaperType(),
            WallpaperType::kDefault);
}

TEST_F(DemoModeIdleHandlerTest, ResetPrefs) {
  auto* pref = profile()->GetPrefs();
  const int default_brightness =
      pref->GetInteger(prefs::kPowerAcScreenBrightnessPercent);

  SimulateUserActivity();
  // Simulate user change screen brightness pref.
  pref->SetInteger(prefs::kPowerAcScreenBrightnessPercent, 10);

  EXPECT_NE(pref->GetInteger(prefs::kPowerAcScreenBrightnessPercent),
            default_brightness);

  FastForwardBy(kReLuanchDemoAppIdleDuration);

  // Expect the pref reset to default after idle.
  EXPECT_EQ(pref->GetInteger(prefs::kPowerAcScreenBrightnessPercent),
            default_brightness);
}

TEST_F(DemoModeIdleHandlerTest, ReLaunchDemoApp) {
  // Clear all immediate task on main thread.
  FastForwardBy(base::Seconds(1));

  // Mock first user interact with device and idle for
  // `kReLuanchDemoAppIdleDuration`:
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  EXPECT_EQ(get_launch_demo_app_count(), 1);

  // Mock a second user come after device idle. App will not launch if duration
  // between 2 activities are less than `kReLuanchDemoAppIdleDuration`.
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration / 2);
  EXPECT_EQ(get_launch_demo_app_count(), 1);
  SimulateUserActivity();
  FastForwardBy(kReLuanchDemoAppIdleDuration / 2 + base::Seconds(1));
  EXPECT_EQ(get_launch_demo_app_count(), 1);

  // Mock no user activity in `kReLuanchDemoAppIdleDuration` + 1 second:
  FastForwardBy(kReLuanchDemoAppIdleDuration + base::Seconds(1));
  // Expect app is launched again:
  EXPECT_EQ(get_launch_demo_app_count(), 2);

  // Mock another idle session without any user activity:
  FastForwardBy(kReLuanchDemoAppIdleDuration);
  // Expect app is not launched:
  EXPECT_EQ(get_launch_demo_app_count(), 2);
}

// DemoIdleHandler test for fallback MGS.
class DemoModeIdleHandlerTestMGS : public DemoModeIdleHandlerTestBase {
  void SetUp() override {
    demo_mode::TurnOnScheduleLogoutForMGS();
    DemoModeIdleHandlerTestBase::SetUp();
  }
};

TEST_F(DemoModeIdleHandlerTestMGS, ScheduleLogout) {
  // Ensure logout in `kLogoutDelayMax`.
  FastForwardBy(kLogoutDelayMax);
  EXPECT_EQ(GetSessionControllerClient()->request_sign_out_count(), 1);
}

TEST_F(DemoModeIdleHandlerTestMGS, LogoutTimerResetOnUserActivity) {
  // TODO(crbugs.com/355727308): `SimulateUserActivity` is not call synchronized
  // here. Figure why.
  demo_mode_idle_handler()->OnUserActivity(nullptr);
  FastForwardBy(kReLuanchDemoAppIdleDuration);

  // Expected the timer is reset.
  EXPECT_FALSE(
      demo_mode_idle_handler()->GetMGSLogoutTimeoutForTest().has_value());
}

}  // namespace ash
