// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_managed_photo_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/managed/screensaver_images_policy_handler.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/test/test_ambient_client.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/ambient/ui/photo_view.h"
#include "ash/assistant/assistant_interaction_controller_impl.h"
#include "ash/constants/ambient_theme.h"
#include "ash/constants/ambient_video.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "ash/login/login_screen_controller.h"
#include "ash/login/ui/lock_screen.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"
#include "ash/public/cpp/personalization_app/time_of_day_paths.h"
#include "ash/public/cpp/test/in_process_image_decoder.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/test_ash_web_view.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "build/buildflag.h"
#include "chromeos/ash/components/assistant/buildflags.h"
#include "chromeos/ash/services/libassistant/public/cpp/assistant_interaction_metadata.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "net/base/url_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/pointer_details.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

using assistant::AssistantInteractionMetadata;

constexpr char kUser1[] = "user1@gmail.com";
constexpr char kUser2[] = "user2@gmail.com";

std::vector<base::OnceClosure> GetEventGeneratorCallbacks(
    ui::test::EventGenerator* event_generator) {
  std::vector<base::OnceClosure> event_callbacks;

  event_callbacks.push_back(
      base::BindOnce(&ui::test::EventGenerator::ClickLeftButton,
                     base::Unretained(event_generator)));

  event_callbacks.push_back(
      base::BindOnce(&ui::test::EventGenerator::ClickRightButton,
                     base::Unretained(event_generator)));

  event_callbacks.push_back(
      base::BindOnce(&ui::test::EventGenerator::DragMouseBy,
                     base::Unretained(event_generator), /*dx=*/10,
                     /*dy=*/10));

  event_callbacks.push_back(
      base::BindOnce(&ui::test::EventGenerator::GestureScrollSequence,
                     base::Unretained(event_generator),
                     /*start=*/gfx::Point(10, 10),
                     /*end=*/gfx::Point(20, 10),
                     /*step_delay=*/base::Milliseconds(10),
                     /*steps=*/1));

  event_callbacks.push_back(
      base::BindOnce(&ui::test::EventGenerator::PressTouch,
                     base::Unretained(event_generator), absl::nullopt));

  return event_callbacks;
}

class AmbientUiVisibilityBarrier : public AmbientUiModelObserver {
 public:
  explicit AmbientUiVisibilityBarrier(AmbientUiVisibility target_visibility)
      : target_visibility_(target_visibility) {
    observation_.Observe(AmbientUiModel::Get());
  }
  AmbientUiVisibilityBarrier(const AmbientUiVisibilityBarrier&) = delete;
  AmbientUiVisibilityBarrier& operator=(const AmbientUiVisibilityBarrier&) =
      delete;
  ~AmbientUiVisibilityBarrier() override = default;

  void WaitWithTimeout(base::TimeDelta timeout) {
    if (AmbientUiModel::Get()->ui_visibility() == target_visibility_)
      return;

    base::test::ScopedRunLoopTimeout run_loop_timeout(FROM_HERE, timeout);
    base::RunLoop run_loop;
    run_loop_quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void OnAmbientUiVisibilityChanged(AmbientUiVisibility visibility) override {
    if (visibility == target_visibility_ && run_loop_quit_closure_) {
      // Post task so that any existing tasks get run before WaitWithTimeout()
      // completes.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(run_loop_quit_closure_));
    }
  }

  const AmbientUiVisibility target_visibility_;
  base::ScopedObservation<AmbientUiModel, AmbientUiModelObserver> observation_{
      this};
  base::RepeatingClosure run_loop_quit_closure_;
};

}  // namespace

class AmbientControllerTest : public AmbientAshTestBase {
 public:
  AmbientControllerTest() = default;
  ~AmbientControllerTest() override = default;

  // AmbientAshTestBase:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kAmbientModeThrottleAnimation);
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
  }

  bool IsPrefObserved(const std::string& pref_name) {
    auto* pref_change_registrar =
        ambient_controller()->pref_change_registrar_.get();
    DCHECK(pref_change_registrar);
    return pref_change_registrar->IsObserved(pref_name);
  }

  bool AreSessionSpecificObserversBound() {
    auto* ctrl = ambient_controller();

    bool ui_model_bound = ctrl->ambient_ui_model_observer_.IsObserving();
    bool backend_model_bound =
        ctrl->ambient_backend_model_observer_.IsObserving();
    bool power_manager_bound =
        ctrl->power_manager_client_observer_.IsObserving();
    bool fingerprint_bound = ctrl->fingerprint_observer_receiver_.is_bound();
    EXPECT_EQ(ui_model_bound, backend_model_bound)
        << "observers should all have the same state";
    EXPECT_EQ(ui_model_bound, power_manager_bound)
        << "observers should all have the same state";
    EXPECT_EQ(ui_model_bound, fingerprint_bound)
        << "observers should all have the same state";
    return ui_model_bound;
  }

  base::test::ScopedFeatureList feature_list_;

 protected:
  base::UserActionTester user_action_tester_;
};

// Tests for behavior that are agnostic to the AmbientUiSettings selected by
// the user should use this test harness.
//
// Currently there are test cases that actually fall under this category but
// do not use this test fixture. This is done purely for time constraint reasons
// (it takes a lot of compute time to repeat every single one of these test
// cases).
class AmbientControllerTestForAnyUiSettings
    : public AmbientControllerTest,
      public ::testing::WithParamInterface<AmbientUiSettings> {
 protected:
  void SetUp() override {
    AmbientControllerTest::SetUp();
    SetAmbientUiSettings(GetParam());
  }
};

INSTANTIATE_TEST_SUITE_P(
    AllUiSettings,
    AmbientControllerTestForAnyUiSettings,
    // Only one lottie-animated theme and video is
    // sufficient here. The main goal here is to make sure
    // that fundamental behavior holds for all themes.
    testing::Values(AmbientUiSettings(AmbientTheme::kSlideshow),
                    AmbientUiSettings(AmbientTheme::kVideo,
                                      AmbientVideo::kNewMexico)
#if BUILDFLAG(HAS_ASH_AMBIENT_ANIMATION_RESOURCES)
                        ,
                    AmbientUiSettings(AmbientTheme::kFeelTheBreeze)
#endif  // BUILDFLAG(HAS_ASH_AMBIENT_ANIMATION_RESOURCES)
                        ),
    [](const ::testing::TestParamInfo<AmbientUiSettings>& param_info) {
      return std::string(ToString(param_info.param.theme()));
    });

TEST_P(AmbientControllerTestForAnyUiSettings, ShowAmbientScreenUponLock) {
  LockScreen();
  // Lockscreen will not immediately show Ambient mode.
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Ambient mode will show after inacivity and successfully loading first
  // image.
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShouldShow);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_P(AmbientControllerTestForAnyUiSettings,
       NotShowAmbientWhenPrefNotEnabled) {
  SetAmbientModeEnabled(false);

  LockScreen();
  // Lockscreen will not immediately show Ambient mode.
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Ambient mode will not show after inacivity and successfully loading first
  // image.
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_TRUE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_P(AmbientControllerTestForAnyUiSettings, HideAmbientScreen) {
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShouldShow);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  HideAmbientScreen();

  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kHidden);

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_P(AmbientControllerTestForAnyUiSettings, CloseAmbientScreenUponUnlock) {
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShouldShow);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  UnlockScreen();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_P(AmbientControllerTestForAnyUiSettings,
       CloseAmbientScreenUponUnlockSecondaryUser) {
  // Simulate the login screen.
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShouldShow);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  SimulateUserLogin(kUser2);
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_F(AmbientControllerTest,
       CloseAmbientScreenUponPowerButtonClickInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  SimulatePowerButtonClick();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // The view should be destroyed along the widget.
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_F(AmbientControllerTest, NotShowAmbientWhenLockSecondaryUser) {
  // Simulate the login screen.
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShouldShow);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  SimulateUserLogin(kUser2);
  SetAmbientModeEnabled(true);

  // Ambient mode should not show for second user even if that user has the pref
  // turned on.
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // The view should be destroyed along the widget.
  FastForwardTiny();
  EXPECT_TRUE(GetContainerViews().empty());

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kClosed);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // The view should be destroyed along the widget.
  EXPECT_TRUE(GetContainerViews().empty());
}

TEST_P(AmbientControllerTestForAnyUiSettings,
       ShouldRequestAccessTokenWhenLockingScreen) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Should close ambient widget already when unlocking screen.
  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_F(AmbientControllerTest, ShouldNotRequestAccessTokenWhenPrefNotEnabled) {
  SetAmbientModeEnabled(false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will not request a token.
  LockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());

  UnlockScreen();
  EXPECT_FALSE(IsAccessTokenRequestPending());
}

TEST_P(AmbientControllerTestForAnyUiSettings, ShouldReturnCachedAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, TestAmbientClient::kTestAccessToken);

        std::move(closure).Run();
        run_loop.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop.Run();

  // Clean up.
  CloseAmbientScreen();
}

// The test body intentionally does not have any actual test expectations. The
// test just has to run without crashing on tear down.
// http://crbug.com/1428481
TEST_P(AmbientControllerTestForAnyUiSettings,
       ShutsDownWithoutCrashingWhileAmbientSessionActive) {
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  // Simulates what happens in a real shutdown scenario. The crash bug above
  // cannot be reproduced without this.
  ClearLogin();
}

TEST_F(AmbientControllerTest, ShouldReturnEmptyAccessToken) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/false);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Another token request will return cached token.
  base::OnceClosure closure = base::MakeExpectedRunClosure(FROM_HERE);
  base::RunLoop run_loop_1;
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_EQ(access_token_fetched, TestAmbientClient::kTestAccessToken);

        std::move(closure).Run();
        run_loop_1.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop_1.Run();

  base::RunLoop run_loop_2;
  // When token expired, another token request will get empty token.
  constexpr base::TimeDelta kTokenRefreshDelay = base::Seconds(60);
  task_environment()->FastForwardBy(kTokenRefreshDelay);

  closure = base::MakeExpectedRunClosure(FROM_HERE);
  ambient_controller()->RequestAccessToken(base::BindLambdaForTesting(
      [&](const std::string& gaia_id, const std::string& access_token_fetched) {
        EXPECT_TRUE(access_token_fetched.empty());

        std::move(closure).Run();
        run_loop_2.Quit();
      }));
  EXPECT_FALSE(IsAccessTokenRequestPending());
  run_loop_2.Run();

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenAfterFailure) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Token request automatically retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenWithBackoffPolicy) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  base::TimeDelta delay1 = GetRefreshTokenDelay();
  task_environment()->FastForwardBy(delay1 * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  base::TimeDelta delay2 = GetRefreshTokenDelay();
  EXPECT_GT(delay2, delay1);

  task_environment()->FastForwardBy(delay2 * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());

  // Clean up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldRetryRefreshAccessTokenOnlyThreeTimes) {
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Lock the screen will request a token.
  LockScreen();
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 1st retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 2nd retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // 3rd retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_TRUE(IsAccessTokenRequestPending());
  IssueAccessToken(/*is_empty=*/true);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  // Will not retry.
  task_environment()->FastForwardBy(GetRefreshTokenDelay() * 1.1);
  EXPECT_FALSE(IsAccessTokenRequestPending());

  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryIsCharging) {
  // Simulate a device being connected to a charger initially.
  SetPowerStateCharging();

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryBatteryIsFullAndDischarging) {
  SetPowerStateDischarging();
  SetBatteryPercent(100.f);
  SetExternalPowerConnected();

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerTest,
       CheckAcquireAndReleaseWakeLockWhenBatteryStateChanged) {
  SetPowerStateDischarging();
  SetExternalPowerConnected();
  SetBatteryPercent(50.f);

  // Lock screen to start ambient mode.
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  // Should not acquire wake lock when device is not charging and with low
  // battery.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Connect the device with a charger.
  SetPowerStateCharging();
  base::RunLoop().RunUntilIdle();

  // Should acquire the wake lock when battery is charging.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Simulates a full battery.
  SetBatteryPercent(100.f);

  // Should keep the wake lock as the charger is still connected.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Disconnects the charger again.
  SetPowerStateDischarging();
  base::RunLoop().RunUntilIdle();

  // Should keep the wake lock when battery is high.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  SetBatteryPercent(50.f);
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when battery is not charging and low.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  SetBatteryPercent(100.f);
  base::RunLoop().RunUntilIdle();

  // Should take the wake lock when battery is not charging and high.
  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  SetExternalPowerDisconnected();
  base::RunLoop().RunUntilIdle();

  // Should release the wake lock when power is not connected.
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // An unbalanced release should do nothing.
  UnlockScreen();
  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_P(AmbientControllerTestForAnyUiSettings, ShouldCloseOnEvent) {
  auto* ambient_ui_model = AmbientUiModel::Get();

  std::vector<base::OnceClosure> event_callbacks =
      GetEventGeneratorCallbacks(GetEventGenerator());

  for (auto& event_callback : event_callbacks) {
    SetAmbientShownAndWaitForWidgets();
    FastForwardTiny();
    EXPECT_TRUE(ambient_controller()->IsShowing());

    std::move(event_callback).Run();

    FastForwardTiny();
    EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
    EXPECT_EQ(AmbientUiVisibility::kClosed, ambient_ui_model->ui_visibility());
    EXPECT_TRUE(GetContainerViews().empty());
  }
}

TEST_P(AmbientControllerTestForAnyUiSettings,
       ShouldDismissToLockScreenOnEvent) {
  auto* ambient_ui_model = AmbientUiModel::Get();

  std::vector<base::OnceClosure> event_callbacks =
      GetEventGeneratorCallbacks(GetEventGenerator());

  for (auto& event_callback : event_callbacks) {
    // Lock screen and fast forward a bit to verify entered hidden state.
    LockScreen();
    FastForwardTiny();
    EXPECT_EQ(AmbientUiVisibility::kHidden, ambient_ui_model->ui_visibility());

    // Wait for timeout to elapse so ambient is shown.
    FastForwardByLockScreenInactivityTimeout();
    EXPECT_EQ(AmbientUiVisibility::kShouldShow,
              ambient_ui_model->ui_visibility());
    EXPECT_TRUE(ambient_controller()->IsShowing());

    // Send an event.
    std::move(event_callback).Run();
    EXPECT_TRUE(GetContainerViews().empty());
    EXPECT_EQ(AmbientUiVisibility::kHidden, ambient_ui_model->ui_visibility());
    // The lock screen timer should have just restarted, so greater than 99% of
    // time remaining on the timer until ambient restarts.
    EXPECT_GT(GetRemainingLockScreenTimeoutFraction().value(), 0.99f);

    // Wait the timeout duration again.
    FastForwardByLockScreenInactivityTimeout();
    FastForwardTiny();
    // Ambient has started again due to elapsed timeout.
    EXPECT_EQ(AmbientUiVisibility::kShouldShow,
              ambient_ui_model->ui_visibility());
    EXPECT_TRUE(ambient_controller()->IsShowing());

    // Reset for next iteration.
    UnlockScreen();
  }
}

// Currently only runs for non-video screen saver settings due to needing to set
// photo download delay.
TEST_F(AmbientControllerTest, ShouldResetLockScreenInactivityTimerOnEvent) {
  auto* ambient_ui_model = AmbientUiModel::Get();
  // Set a long photo download delay so that state is
  // `AmbientUiVisibility::kShown` but widget does not exist to receive events
  // yet.
  SetPhotoDownloadDelay(base::Seconds(1));
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  // Ambient controller is shown but photos have not yet downloaded, so ambient
  // widget and container views do not exist.
  EXPECT_EQ(AmbientUiVisibility::kShouldShow,
            ambient_ui_model->ui_visibility());
  EXPECT_FALSE(ambient_controller()->IsShowing())
      << "Ambient container views should not exist because photos not yet "
         "downloaded";
  // Inactivity timer has elapsed so nullopt.
  EXPECT_FALSE(GetRemainingLockScreenTimeoutFraction().has_value());

  // Send a user activity through `UserActivityDetector`. `EventGenerator` is
  // not hooked up to `UserActivityDetector` in this test environment, so
  // manually trigger `UserActivityDetector` ourselves.
  auto* user_activity_detector = ui::UserActivityDetector::Get();
  ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  user_activity_detector->DidProcessEvent(&event);
  EXPECT_EQ(AmbientUiVisibility::kShouldShow, ambient_ui_model->ui_visibility())
      << "Still shown because waiting for `OnKeyEvent` to be called";

  // Call `OnKeyEvent` via `EventGenerator`.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);
  EXPECT_EQ(AmbientUiVisibility::kHidden, ambient_ui_model->ui_visibility())
      << "Should be kHidden because of recent OnKeyEvent call";
  EXPECT_GT(GetRemainingLockScreenTimeoutFraction().value(), 0.99)
      << "Lock screen inactivity timer should have restarted";

  FastForwardByLockScreenInactivityTimeout(0.5);
  EXPECT_GT(GetRemainingLockScreenTimeoutFraction().value(), 0.4);

  FastForwardByLockScreenInactivityTimeout(0.51);
  EXPECT_FALSE(GetRemainingLockScreenTimeoutFraction().has_value())
      << "Inactivity timer has stopped";
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  EXPECT_FALSE(ambient_controller()->IsShowing())
      << "Photos still have not yet downloaded";

  task_environment()->FastForwardBy(base::Seconds(2));
  // Finally visible and running now that images downloaded.
  EXPECT_TRUE(ambient_controller()->IsShowing());
}

TEST_P(AmbientControllerTestForAnyUiSettings,
       ShouldDismissContainerViewOnKeyEvent) {
  // Without user interaction, should show ambient mode.
  SetAmbientShownAndWaitForWidgets();
  EXPECT_TRUE(ambient_controller()->IsShowing());
  CloseAmbientScreen();

  // When ambient is shown, OnUserActivity() should ignore key event.
  ambient_controller()->SetUiVisibilityShown();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // General key press will exit ambient mode.
  // Simulate key press to close the widget.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldDismissContainerViewOnKeyEventWhenLockScreenInBackground) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByBackgroundLockScreenTimeout();
  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // General key press will exit ambient mode.
  // Simulate key press to close the widget.
  PressAndReleaseKey(ui::VKEY_A);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByBackgroundLockScreenTimeout();
  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithLockscreenWithNoisyPowerEvents) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should enter ambient mode when the screen is dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByBackgroundLockScreenTimeout(0.5001);
  SetPowerStateCharging();

  FastForwardByBackgroundLockScreenTimeout(0.5001);
  SetPowerStateCharging();

  EXPECT_TRUE(IsLocked());
  // Should not disrupt ongoing ambient mode.
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Closes ambient for clean-up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldShowAmbientScreenWithoutLockscreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  // When power is discharging, we do not lock the screen with ambient
  // mode since we do not prevent the device go to sleep which will natually
  // lock the device.
  SetPowerStateDischarging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device but still enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByBackgroundLockScreenTimeout();
  EXPECT_FALSE(IsLocked());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldShowAmbientScreenWhenScreenIsDimmed) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device but enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByBackgroundLockScreenTimeout();
  EXPECT_FALSE(IsLocked());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, HandlesPreviousImageFailuresWithLockScreen) {
  // Simulate failures to download FIFE urls. Ambient mode should close and
  // remember the old failure.
  SetDownloadPhotoData("");
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  AmbientUiVisibilityBarrier ambient_closed_barrier(
      AmbientUiVisibility::kClosed);
  ambient_closed_barrier.WaitWithTimeout(base::Seconds(15));
  ASSERT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  UnlockScreen();

  // Now simulate FIFE downloads starting to work again. The device should be
  // able to enter ambient mode.
  ClearDownloadPhotoData();
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, HandlesPreviousImageFailuresWithDimmedScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  SetPowerStateCharging();

  // Simulate failures to download FIFE urls. Ambient mode should close and
  // remember the old failure.
  SetDownloadPhotoData("");
  SetScreenIdleStateAndWait(/*is_screen_dimmed=*/true, /*is_off=*/false);
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  AmbientUiVisibilityBarrier ambient_closed_barrier(
      AmbientUiVisibility::kClosed);
  ambient_closed_barrier.WaitWithTimeout(base::Seconds(15));
  ASSERT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  SetScreenIdleStateAndWait(/*is_screen_dimmed=*/false, /*is_off=*/false);

  // Usually would enter ambient mode when the screen is dimmed, but this time
  // it shouldn't because of the previous image failures.
  SetScreenIdleStateAndWait(/*is_screen_dimmed=*/true, /*is_off=*/false);
  FastForwardTiny();
  ASSERT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  SetScreenIdleStateAndWait(/*is_screen_dimmed=*/false, /*is_off=*/false);

  // Now simulate FIFE downloads starting to work again. The device should be
  // able to enter ambient mode.
  ClearDownloadPhotoData();
  SetScreenIdleStateAndWait(/*is_screen_dimmed=*/true, /*is_off=*/false);
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Closes ambient for clean-up.
  CloseAmbientScreen();
}

TEST_F(AmbientControllerTest, ShouldHideAmbientScreenWhenDisplayIsOff) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(false);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Should dismiss ambient mode screen.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Screen back on again, should not have ambient screen.
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldHideAmbientScreenWhenDisplayIsOffThenComesBackWithLockScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateCharging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByBackgroundLockScreenTimeout();
  EXPECT_TRUE(IsLocked());

  // Should dismiss ambient mode screen.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Screen back on again, should not have ambient screen, but still has lock
  // screen.
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldHideAmbientScreenWhenDisplayIsOffAndNotStartWhenLockScreen) {
  GetSessionControllerClient()->SetShouldLockScreenAutomatically(true);
  SetPowerStateDischarging();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device and enter ambient mode when the screen is
  // dimmed.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/false);
  EXPECT_FALSE(IsLocked());

  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Should not lock the device because the device is not charging.
  FastForwardByBackgroundLockScreenTimeout();
  EXPECT_FALSE(IsLocked());

  // Should dismiss ambient mode screen.
  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Lock screen will not start ambient mode.
  LockScreen();
  EXPECT_TRUE(IsLocked());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Screen back on again, should not have ambient screen, but still has lock
  // screen.
  SetScreenIdleStateAndWait(/*dimmed=*/false, /*off=*/false);
  EXPECT_TRUE(IsLocked());
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, HandlesPhotoDownloadOutage) {
  SetDownloadPhotoData("");

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  AmbientUiVisibilityBarrier ambient_closed_barrier(
      AmbientUiVisibility::kClosed);
  ambient_closed_barrier.WaitWithTimeout(base::Seconds(15));
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_P(AmbientControllerTestForAnyUiSettings, HideCursor) {
  auto* cursor_manager = Shell::Get()->cursor_manager();
  LockScreen();

  cursor_manager->ShowCursor();
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(GetContainerViews().empty());
  EXPECT_EQ(AmbientUiModel::Get()->ui_visibility(),
            AmbientUiVisibility::kShouldShow);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  EXPECT_FALSE(cursor_manager->IsCursorVisible());

  // Clean up.
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_P(AmbientControllerTestForAnyUiSettings, ShowsOnMultipleDisplays) {
  UpdateDisplay("800x600,800x600");
  FastForwardTiny();

  SetAmbientShownAndWaitForWidgets();

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetNumDisplays(), 2);
  EXPECT_EQ(GetContainerViews().size(), 2u);
  AmbientViewID expected_child_view_id;
  switch (GetParam().theme()) {
    case AmbientTheme::kVideo:
      expected_child_view_id = kAmbientVideoWebView;
      break;
    case AmbientTheme::kSlideshow:
      expected_child_view_id = AmbientViewID::kAmbientPhotoView;
      break;
    case AmbientTheme::kFeelTheBreeze:
    case AmbientTheme::kFloatOnBy:
      expected_child_view_id = AmbientViewID::kAmbientAnimationView;
      break;
  }
  EXPECT_TRUE(GetContainerViews().front()->GetViewByID(expected_child_view_id));
  EXPECT_TRUE(GetContainerViews().back()->GetViewByID(expected_child_view_id));
  // Check that each root controller has an ambient widget.
  for (auto* ctrl : RootWindowController::root_window_controllers())
    EXPECT_TRUE(ctrl->ambient_widget_for_testing() &&
                ctrl->ambient_widget_for_testing()->IsVisible());
}

TEST_P(AmbientControllerTestForAnyUiSettings, RespondsToDisplayAdded) {
  // UpdateDisplay triggers a rogue MouseEvent that cancels Ambient mode when
  // testing with Xvfb. A corresponding MouseEvent is not fired on a real device
  // when an external display is added. Ignore this MouseEvent for testing.
  // Store the old |ShouldIgnoreNativePlatformEvents| value and reset it at the
  // end of the test.
  bool old_should_ignore_events =
      ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents();
  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(true);

  UpdateDisplay("800x600");
  SetAmbientShownAndWaitForWidgets();

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetNumDisplays(), 1);
  EXPECT_EQ(GetContainerViews().size(), 1u);

  UpdateDisplay("800x600,800x600");
  FastForwardTiny();

  EXPECT_TRUE(ambient_controller()->IsShowing());
  EXPECT_EQ(screen->GetNumDisplays(), 2);
  EXPECT_EQ(GetContainerViews().size(), 2u);
  for (auto* ctrl : RootWindowController::root_window_controllers())
    EXPECT_TRUE(ctrl->ambient_widget_for_testing() &&
                ctrl->ambient_widget_for_testing()->IsVisible());

  ui::PlatformEventSource::SetIgnoreNativePlatformEvents(
      old_should_ignore_events);
}

TEST_P(AmbientControllerTestForAnyUiSettings, HandlesDisplayRemoved) {
  UpdateDisplay("800x600,800x600");
  FastForwardTiny();

  SetAmbientShownAndWaitForWidgets();

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetNumDisplays(), 2);
  EXPECT_EQ(GetContainerViews().size(), 2u);
  EXPECT_TRUE(ambient_controller()->IsShowing());

  // Changing to one screen will destroy the widget on the non-primary screen.
  UpdateDisplay("800x600");
  FastForwardTiny();

  EXPECT_EQ(screen->GetNumDisplays(), 1);
  EXPECT_EQ(GetContainerViews().size(), 1u);
  EXPECT_TRUE(ambient_controller()->IsShowing());
}

TEST_F(AmbientControllerTest, ClosesAmbientBeforeSuspend) {
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  SimulateSystemSuspendAndWait(power_manager::SuspendImminent::Reason::
                                   SuspendImminent_Reason_LID_CLOSED);

  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  FastForwardByLockScreenInactivityTimeout();
  // Ambient mode should not resume until SuspendDone is received.
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, RestartsAmbientAfterSuspend) {
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  SimulateSystemSuspendAndWait(
      power_manager::SuspendImminent::Reason::SuspendImminent_Reason_IDLE);

  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // This call should be blocked by prior |SuspendImminent| until |SuspendDone|.
  ambient_controller()->SetUiVisibilityShown();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  SimulateSystemResumeAndWait();

  FastForwardByLockScreenInactivityTimeout();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, ObservesPrefsWhenAmbientEnabled) {
  SetAmbientModeEnabled(false);

  // This pref is always observed.
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  std::vector<std::string> other_prefs{
      ambient::prefs::kAmbientModeLockScreenInactivityTimeoutSeconds,
      ambient::prefs::kAmbientModeLockScreenBackgroundTimeoutSeconds,
      ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds};

  for (auto& pref_name : other_prefs)
    EXPECT_FALSE(IsPrefObserved(pref_name));

  SetAmbientModeEnabled(true);

  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  for (auto& pref_name : other_prefs)
    EXPECT_TRUE(IsPrefObserved(pref_name));
}

TEST_F(AmbientControllerTest, BindsObserversWhenAmbientEnabled) {
  auto* ctrl = ambient_controller();

  SetAmbientModeEnabled(false);

  // SessionObserver must always be observing to detect when user pref service
  // is started.
  EXPECT_TRUE(ctrl->session_observer_.IsObserving());

  EXPECT_FALSE(AreSessionSpecificObserversBound());

  SetAmbientModeEnabled(true);

  // Session observer should still be observing.
  EXPECT_TRUE(ctrl->session_observer_.IsObserving());

  EXPECT_TRUE(AreSessionSpecificObserversBound());
}

TEST_F(AmbientControllerTest, SwitchActiveUsersDoesNotDoubleBindObservers) {
  ClearLogin();
  SimulateUserLogin(kUser1);
  SetAmbientModeEnabled(true);

  TestSessionControllerClient* session = GetSessionControllerClient();

  // Observers are bound for primary user with Ambient mode enabled.
  EXPECT_TRUE(AreSessionSpecificObserversBound());
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  // Observers are still bound when secondary user logs in.
  SimulateUserLogin(kUser2);
  EXPECT_TRUE(AreSessionSpecificObserversBound());
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  // Observers are not re-bound for primary user when session is active.
  session->SwitchActiveUser(AccountId::FromUserEmail(kUser1));
  EXPECT_TRUE(AreSessionSpecificObserversBound());
  EXPECT_TRUE(IsPrefObserved(ambient::prefs::kAmbientModeEnabled));

  //  Switch back to secondary user.
  session->SwitchActiveUser(AccountId::FromUserEmail(kUser2));
}

TEST_F(AmbientControllerTest, BindsObserversWhenAmbientOn) {
  auto* ctrl = ambient_controller();

  LockScreen();

  // Start monitoring user activity on hidden ui.
  EXPECT_TRUE(ctrl->user_activity_observer_.IsObserving());
  // Do not monitor power status yet.
  EXPECT_FALSE(ctrl->power_status_observer_.IsObserving());

  FastForwardByLockScreenInactivityTimeout();

  EXPECT_TRUE(ctrl->user_activity_observer_.IsObserving());
  EXPECT_TRUE(ctrl->power_status_observer_.IsObserving());

  UnlockScreen();

  EXPECT_FALSE(ctrl->user_activity_observer_.IsObserving());
  EXPECT_FALSE(ctrl->power_status_observer_.IsObserving());
}

TEST_P(AmbientControllerTestForAnyUiSettings,
       ShowDismissAmbientScreenUponAssistantQuery) {
  // Without user interaction, should show ambient mode.
  SetAmbientShownAndWaitForWidgets();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  // Trigger Assistant interaction.
  static_cast<AssistantInteractionControllerImpl*>(
      AssistantInteractionController::Get())
      ->OnInteractionStarted(AssistantInteractionMetadata());
  base::RunLoop().RunUntilIdle();

  // Ambient screen should dismiss.
  EXPECT_TRUE(GetContainerViews().empty());
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

// For all test cases that depend on ash ambient resources (lottie files, image
// assets, etc) being present to run.
#if BUILDFLAG(HAS_ASH_AMBIENT_ANIMATION_RESOURCES)
#define ANIMATION_TEST_WITH_RESOURCES(test_case_name) test_case_name
#else
#define ANIMATION_TEST_WITH_RESOURCES(test_case_name) DISABLED_##test_case_name
#endif  // BUILDFLAG(HAS_ASH_AMBIENT_ANIMATION_RESOURCES)

TEST_F(AmbientControllerTest,
       ANIMATION_TEST_WITH_RESOURCES(RendersCorrectView)) {
  SetAmbientTheme(AmbientTheme::kFeelTheBreeze);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  EXPECT_FALSE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientAnimationView));

  UnlockScreen();
  SetAmbientTheme(AmbientTheme::kSlideshow);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  EXPECT_FALSE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientAnimationView));

  UnlockScreen();
  SetAmbientTheme(AmbientTheme::kFeelTheBreeze);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  EXPECT_FALSE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientAnimationView));
}

TEST_F(AmbientControllerTest,
       ANIMATION_TEST_WITH_RESOURCES(ClearsCacheWhenSwitchingThemes)) {
  SetAmbientTheme(AmbientTheme::kSlideshow);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  ASSERT_FALSE(GetCachedFiles().empty());

  UnlockScreen();
  SetAmbientTheme(AmbientTheme::kFeelTheBreeze);

  // Mimic a network outage where no photos can be downloaded. Since the cache
  // should have been cleared when we switched ambient animation themes, the
  // UI shouldn't start with a photo cached during slideshow mode.
  SetDownloadPhotoData(/*data=*/"");
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_FALSE(GetContainerView());
  EXPECT_TRUE(GetCachedFiles().empty());
}

TEST_F(AmbientControllerTest,
       ANIMATION_TEST_WITH_RESOURCES(MetricsStartupTimeSuspendAfterTimeMax)) {
  SetAmbientTheme(AmbientTheme::kSlideshow);
  base::HistogramTester histogram_tester;
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  task_environment()->FastForwardBy(ambient::kMetricsStartupTimeMax);
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  SimulateSystemSuspendAndWait(power_manager::SuspendImminent::Reason::
                                   SuspendImminent_Reason_LID_CLOSED);

  ASSERT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  histogram_tester.ExpectTotalCount("Ash.AmbientMode.StartupTime.SlideShow", 1);
  UnlockScreen();
}

TEST_F(AmbientControllerTest,
       ANIMATION_TEST_WITH_RESOURCES(MetricsStartupTimeScreenOffAfterTimeMax)) {
  SetAmbientTheme(AmbientTheme::kSlideshow);
  base::HistogramTester histogram_tester;
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();

  task_environment()->FastForwardBy(ambient::kMetricsStartupTimeMax);
  FastForwardTiny();
  ASSERT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  SetScreenIdleStateAndWait(/*dimmed=*/true, /*off=*/true);

  ASSERT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  histogram_tester.ExpectTotalCount("Ash.AmbientMode.StartupTime.SlideShow", 1);
  UnlockScreen();
}

TEST_F(AmbientControllerTest, ShouldStartScreenSaverPreview) {
  ASSERT_EQ(0,
            user_action_tester_.GetActionCount(kScreenSaverPreviewUserAction));
  ambient_controller()->SetUiVisibilityPreview();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  EXPECT_FALSE(IsLocked());
  EXPECT_EQ(1,
            user_action_tester_.GetActionCount(kScreenSaverPreviewUserAction));
}

TEST_F(AmbientControllerTest,
       ShouldNotDismissScreenSaverPreviewOnUserActivity) {
  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  ui::MouseEvent mouse_event(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                             base::TimeTicks(), ui::EF_NONE, ui::EF_NONE);
  ui::UserActivityDetector::Get()->DidProcessEvent(&mouse_event);
  FastForwardTiny();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, ShouldDismissScreenSaverPreviewOnKeyReleased) {
  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->ReleaseKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->PressKey(ui::VKEY_A, ui::EF_NONE);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest,
       ShouldNotDismissScreenSaverPreviewOnSomeMouseEvents) {
  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->MoveMouseWheel(10, 10);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->SendMouseEnter();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->SendMouseExit();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, ShouldDismissScreenSaverPreviewOnMouseClick) {
  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->ClickRightButton();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, MaybeDismissUIOnMouseMove) {
  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 5), /*count=*/2);
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  task_environment()->FastForwardBy(kDismissPreviewOnMouseMoveDelay);
  FastForwardTiny();
  GetEventGenerator()->MoveMouseTo(gfx::Point(5, 5), /*count=*/2);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, ShouldDismissScreenSaverPreviewOnTouch) {
  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->PressTouch();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  ambient_controller()->SetUiVisibilityPreview();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());

  GetEventGenerator()->ReleaseTouch();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

class AmbientControllerForManagedScreensaverTest : public AmbientAshTestBase {
 public:
  AmbientControllerForManagedScreensaverTest() {
    CreateTestData();
    // Required as otherwise the PathService::CheckedGet fails in the
    // screensaver images policy handler.
    device_policy_screensaver_folder_override_ =
        std::make_unique<base::ScopedPathOverride>(
            ash::DIR_DEVICE_POLICY_SCREENSAVER_DATA, temp_dir_.GetPath());
  }
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kAmbientModeManagedScreensaver);
    AmbientAshTestBase::SetUp();
    // Disable consumer ambient mode
    SetAmbientModeEnabled(false);
    GetSessionControllerClient()->set_show_lock_screen_views(true);
  }

  void TearDown() override {
    image_file_paths_.clear();
    AmbientAshTestBase::TearDown();
  }

 protected:
  void CreateTestData() {
    bool success = temp_dir_.CreateUniqueTempDir();
    ASSERT_TRUE(success);
    base::FilePath image_1 =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("IMAGE_1.jpg"));
    CreateTestImageJpegFile(image_1, 4, 4, SK_ColorRED);
    base::FilePath image_2 =
        temp_dir_.GetPath().Append(FILE_PATH_LITERAL("IMAGE_2.jpg"));
    CreateTestImageJpegFile(image_2, 8, 8, SK_ColorGREEN);

    image_file_paths_.push_back(image_1);
    image_file_paths_.push_back(image_2);
  }

  void SimulateScreensaverStart() {
    LockScreen();
    FastForwardByLockScreenInactivityTimeout();
    EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  InProcessImageDecoder decoder_;
  std::vector<base::FilePath> image_file_paths_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<base::ScopedPathOverride>
      device_policy_screensaver_folder_override_;
};

TEST_F(AmbientControllerForManagedScreensaverTest,
       ScreensaverIsShownWithEnoughImages) {
  SetAmbientModeManagedScreensaverEnabled(true);

  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  SimulateScreensaverStart();

  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));

  // Peripheral Ui is always hidden in managed screeensaver mode
  EXPECT_FALSE(GetAmbientSlideshowPeripheralUi()->GetVisible())
      << "Peripheral Ui should be hidden in managed mode";

  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  ASSERT_FALSE(GetContainerView());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       ScreensaverIsNotShownWithoutImages) {
  SetAmbientModeManagedScreensaverEnabled(true);
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_FALSE(GetContainerView());
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       UiLauncherIsNullWhenManagedAmbientModeIsDisabled) {
  SetAmbientModeEnabled(false);
  SetAmbientModeManagedScreensaverEnabled(false);

  ASSERT_FALSE(ambient_controller()->ambient_ui_launcher());

  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       DisablingManagedAmbientModeFallsbackToUserAmbientModeIfEnabled) {
  SetAmbientModeEnabled(true);
  SetAmbientModeManagedScreensaverEnabled(true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  SimulateScreensaverStart();
  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  SetAmbientModeManagedScreensaverEnabled(false);
  DisableBackupCacheDownloads();
  UnlockScreen();

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  EXPECT_TRUE(GetAmbientSlideshowPeripheralUi()->GetVisible());
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       LaunchingManagedAmbientModeAfterAmbientModeWorksAsExpected) {
  SetAmbientModeEnabled(/*enabled=*/true);
  ASSERT_FALSE(ambient_controller()->ambient_ui_launcher());
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  ASSERT_TRUE(ambient_controller()->ambient_ui_launcher());

  managed_policy_handler()->SetImagesForTesting(image_file_paths_);

  SimulateScreensaverStart();
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       LaunchingAmbientModeAfterManagedAmbientModeWorksAsExpected) {
  SetAmbientModeEnabled(/*enabled=*/false);
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  SetAmbientModeEnabled(/*enabled=*/true);

  managed_policy_handler()->SetImagesForTesting(image_file_paths_);

  SimulateScreensaverStart();
  UnlockScreen();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverTest, PrefObserverUpdatesUiModel) {
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  ASSERT_TRUE(ambient_controller()->ambient_ui_launcher());
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  AmbientUiModel* ui_model = ambient_controller()->ambient_ui_model();
  constexpr size_t kExpectedIdleTimeout = 55;
  constexpr size_t kExpectedPhotoRefreshInterval = 77;
  pref_service->SetInteger(
      ambient::prefs::kAmbientModeManagedScreensaverIdleTimeoutSeconds,
      kExpectedIdleTimeout);
  EXPECT_EQ(base::Seconds(kExpectedIdleTimeout),
            ui_model->lock_screen_inactivity_timeout());
  pref_service->SetInteger(
      ambient::prefs::kAmbientModeManagedScreensaverImageDisplayIntervalSeconds,
      kExpectedPhotoRefreshInterval);
  EXPECT_EQ(base::Seconds(kExpectedPhotoRefreshInterval),
            ui_model->photo_refresh_interval());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       WorksWithAmbientManagedPhotoSource) {
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);

  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  SimulateScreensaverStart();

  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  UnlockScreen();

  ASSERT_FALSE(GetContainerView());
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  SimulateScreensaverStart();
  // Will start as there are images present already
  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       ManagedAmbientModeGetsEnabledOnLockScreenAndStartsIt) {
  LockScreen();
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  FastForwardByLockScreenInactivityTimeout();
  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
}

class AmbientControllerForManagedScreensaverLoginScreenTest
    : public AmbientControllerForManagedScreensaverTest {
 public:
  void SetUp() override {
    // For login screen tests we don't want to start a session rather we want to
    // start on the login screen.
    set_start_session(false);
    AmbientControllerForManagedScreensaverTest::SetUp();
    SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
    managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  }

  void TriggerLoginScreen() {
    GetSessionControllerClient()->RequestSignOut();
    // The login screen can't be shown without a wallpaper.
    Shell::Get()->wallpaper_controller()->ShowDefaultWallpaperForTesting();
    Shell::Get()->login_screen_controller()->ShowLoginScreen();
    GetSessionControllerClient()->FlushForTest();
    FastForwardByLockScreenInactivityTimeout();
  }
};

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       ShownOnLoginScreen) {
  TriggerLoginScreen();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       ShownOnLoginWhenPrefUpdatedLater) {
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/false);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  // Login screen is shown when the managed mode is disabled
  TriggerLoginScreen();
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
}

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       NotShownOnLoginScreenWhenDisabled) {
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/false);
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       UserLogsInAmbientModeDisabledAndManagedAmbientModeEnabled) {
  TriggerLoginScreen();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());

  // Simulate user session start (e.g. user login)
  CreateUserSessions(/*session_count=*/1);

  // Confirm that ambient mode is not shown if disabled. (disabled by default)
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_FALSE(GetContainerView());
  ASSERT_FALSE(ambient_controller()->ambient_ui_launcher());

  // Enabling and locking screen starts the managed ambient mode
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
}

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       UserLogsInAmbientModeEnabled) {
  TriggerLoginScreen();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());

  // Simulate user session start (e.g. consumer user login)
  SimulateNewUserFirstLogin(kUser1);

  // Enabling and locking screen starts the consumer ambient mode
  SetAmbientModeEnabled(true);
  DisableBackupCacheDownloads();
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
}

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       ManagedScreensaverClosedWhenImagesCleared) {
  TriggerLoginScreen();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
  // Clear images
  managed_policy_handler()->SetImagesForTesting({});
  EXPECT_FALSE(ambient_controller()->IsShowing());
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Simulate login
  CreateUserSessions(/*session_count=*/1);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  SetAmbientModeManagedScreensaverEnabled(true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());

  managed_policy_handler()->SetImagesForTesting({});
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
  FastForwardByLockScreenInactivityTimeout();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverLoginScreenTest,
       ManagedScreensaverClosedWhenImageLoadingFails) {
  TriggerLoginScreen();
  EXPECT_TRUE(ambient_controller()->ShouldShowAmbientUi());
  ASSERT_TRUE(GetContainerView());
  // Set invalid images ( i.e. either the paths are invalid or images themselves
  // have been deleted).
  std::vector<base::FilePath> invalid_image_paths = {
      base::FilePath(FILE_PATH_LITERAL("invalid_path_1")),
      base::FilePath(FILE_PATH_LITERAL("invalid_path_2"))};
  managed_policy_handler()->SetImagesForTesting(invalid_image_paths);
  // Fast forward a tiny amount to run any async tasks.
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  // Simulate login
  CreateUserSessions(/*session_count=*/1);
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());

  SetAmbientModeManagedScreensaverEnabled(true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  SimulateScreensaverStart();
  EXPECT_TRUE(ambient_controller()->IsShowing());
  managed_policy_handler()->SetImagesForTesting(invalid_image_paths);
  // Fast forward a tiny amount to run any async tasks.
  FastForwardTiny();
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerForManagedScreensaverTest,
       ManagedScreensaverNotShownOnScreenDim) {
  SetAmbientModeManagedScreensaverEnabled(/*enabled=*/true);
  managed_policy_handler()->SetImagesForTesting(image_file_paths_);
  SetScreenIdleStateAndWait(/*is_screen_dimmed=*/true, /*is_off=*/false);
  EXPECT_FALSE(IsLocked());
  EXPECT_FALSE(ambient_controller()->ShouldShowAmbientUi());
}

TEST_F(AmbientControllerTest, RendersCorrectViewForVideo) {
  SetAmbientUiSettings(
      AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kNewMexico));

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  const TestAshWebView* web_view = static_cast<const TestAshWebView*>(
      GetContainerView()->GetViewByID(kAmbientVideoWebView));
  ASSERT_TRUE(web_view);
  EXPECT_TRUE(web_view->current_url().SchemeIsFile());
  EXPECT_EQ(web_view->current_url().path(),
            personalization_app::GetTimeOfDaySrcDir()
                .Append(personalization_app::kAmbientVideoHtml)
                .value());
  std::string video_file_requested;
  ASSERT_TRUE(net::GetValueForKeyInQuery(web_view->current_url(), "video_file",
                                         &video_file_requested));
  EXPECT_EQ(video_file_requested,
            personalization_app::kTimeOfDayNewMexicoVideo);

  UnlockScreen();
  SetAmbientTheme(AmbientTheme::kSlideshow);

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  EXPECT_TRUE(
      GetContainerView()->GetViewByID(AmbientViewID::kAmbientPhotoView));

  UnlockScreen();
  SetAmbientUiSettings(
      AmbientUiSettings(AmbientTheme::kVideo, AmbientVideo::kClouds));

  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  ASSERT_TRUE(GetContainerView());
  web_view = static_cast<const TestAshWebView*>(
      GetContainerView()->GetViewByID(kAmbientVideoWebView));
  ASSERT_TRUE(web_view);
  EXPECT_TRUE(web_view->current_url().SchemeIsFile());
  EXPECT_EQ(web_view->current_url().path(),
            personalization_app::GetTimeOfDaySrcDir()
                .Append(personalization_app::kAmbientVideoHtml)
                .value());
  ASSERT_TRUE(net::GetValueForKeyInQuery(web_view->current_url(), "video_file",
                                         &video_file_requested));
  EXPECT_EQ(video_file_requested, personalization_app::kTimeOfDayCloudsVideo);
}

class AmbientControllerDurationTest : public AmbientAshTestBase {
 public:
  AmbientControllerDurationTest() = default;
  ~AmbientControllerDurationTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kScreenSaverDuration);
    AmbientAshTestBase::SetUp();
    GetSessionControllerClient()->set_show_lock_screen_views(true);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AmbientControllerDurationTest, SetScreenSaverDuration) {
  EXPECT_TRUE(ash::features::IsScreenSaverDurationEnabled());

  // Set screen saver duration.
  SetAmbientModeEnabled(true);
  SetScreenSaverDuration(5);
  EXPECT_EQ(5, GetScreenSaverDuration());

  SetScreenSaverDuration(10);
  EXPECT_EQ(10, GetScreenSaverDuration());

  SetScreenSaverDuration(0);
  EXPECT_EQ(0, GetScreenSaverDuration());
}

TEST_F(AmbientControllerDurationTest, AcquireWakeLockWithoutCharger) {
  // Simulate User logged in.
  ClearLogin();
  SimulateUserLogin(kUser1);

  // Set screen saver duration to forever.
  SetAmbientModeEnabled(true);
  SetScreenSaverDuration(0);
  EXPECT_EQ(0, GetScreenSaverDuration());

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

TEST_F(AmbientControllerDurationTest, AcquireWakeLockWithCharger) {
  // Simulate User logged in.
  ClearLogin();
  SimulateUserLogin(kUser1);

  // Set screen saver duration to forever.
  SetAmbientModeEnabled(true);
  SetScreenSaverDuration(0);
  EXPECT_EQ(0, GetScreenSaverDuration());

  // Simulate a device being connected to a charger initially.
  SetPowerStateCharging();

  // Lock screen to start ambient mode, and flush the loop to ensure
  // the acquire wake lock request has reached the wake lock provider.
  LockScreen();
  FastForwardByLockScreenInactivityTimeout();
  FastForwardTiny();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  HideAmbientScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Ambient screen showup again after inactivity.
  FastForwardByLockScreenInactivityTimeout();

  EXPECT_EQ(1, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));

  // Unlock screen to exit ambient mode.
  UnlockScreen();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0, GetNumOfActiveWakeLocks(
                   device::mojom::WakeLockType::kPreventDisplaySleep));
}

}  // namespace ash
