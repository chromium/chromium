// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/curtain/security_curtain_controller.h"
#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/power/power_button_controller_test_api.h"
#include "ash/system/power/power_button_menu_view.h"
#include "ash/system/power/power_button_test_base.h"
#include "ash/system/privacy_hub/camera_privacy_switch_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/manager/display_manager.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

namespace aura {
// This improves the error output for our tests that compare `OcclusionState`.
std::ostream& operator<<(std::ostream& os, Window::OcclusionState state) {
  os << Window::OcclusionStateToString(state);
  return os;
}
}  // namespace aura

namespace ash::curtain {
namespace {

using ::testing::Eq;
using ::testing::Ne;

using DisplayId = uint64_t;

ViewFactory FakeViewFactory() {
  return base::BindRepeating(
      []() { return views::Builder<views::View>().Build(); });
}

}  // namespace

class SecurityCurtainControllerImplTest : public PowerButtonTestBase {
 public:
  SecurityCurtainControllerImplTest()
      : PowerButtonTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  SecurityCurtainControllerImplTest(const SecurityCurtainControllerImplTest&) =
      delete;
  SecurityCurtainControllerImplTest& operator=(
      const SecurityCurtainControllerImplTest&) = delete;
  ~SecurityCurtainControllerImplTest() override = default;

  void SetUp() override {
    PowerButtonTestBase::SetUp();
    InitPowerButtonControllerMembers(
        chromeos::PowerManagerClient::TabletMode::UNSUPPORTED);
    power_button_test_api_ = std::make_unique<PowerButtonControllerTestApi>(
        Shell::Get()->power_button_controller());
  }

  void TearDown() override {
    power_button_test_api_.reset();
    ResetPowerButtonController();
    PowerButtonTestBase::TearDown();
  }

  SecurityCurtainController& security_curtain_controller() {
    return ash::Shell::Get()->security_curtain_controller();
  }

  CameraPrivacySwitchController& camera_controller() {
    return CHECK_DEREF(CameraPrivacySwitchController::Get());
  }

  PowerButtonControllerTestApi& power_button_test_api() {
    return *power_button_test_api_;
  }

  const ui::InputController& input_controller() {
    return CHECK_DEREF(ui::OzonePlatform::GetInstance()->GetInputController());
  }

  SecurityCurtainController::InitParams init_params() {
    return SecurityCurtainController::InitParams{FakeViewFactory()};
  }

  bool IsNativeCursorEnabled() {
    return !Shell::Get()
                ->window_tree_host_manager()
                ->cursor_window_controller()
                ->is_cursor_compositing_enabled();
  }

  SecurityCurtainController::InitParams WithViewFactory(ViewFactory factory) {
    return SecurityCurtainController::InitParams{factory};
  }

  bool IsCurtainShownOnDisplay(const display::Display& display) {
    return IsCurtainShownOnDisplay(display.id());
  }

  bool IsCurtainShownOnDisplay(DisplayId display_id) {
    auto* root_window_controller =
        Shell::GetRootWindowControllerWithDisplayId(display_id);
    if (!root_window_controller) {
      return false;
    }

    const auto* controller =
        root_window_controller->security_curtain_widget_controller();

    return controller != nullptr;
  }

  views::Widget& GetCurtainForDisplay(const display::Display& display) {
    auto* controller = Shell::GetRootWindowControllerWithDisplayId(display.id())
                           ->security_curtain_widget_controller();
    EXPECT_THAT(controller, testing::NotNull())
        << "Missing curtain widget for display " << display.ToString();

    return controller->GetWidget();
  }

  display::Displays GetDisplays() {
    return display_manager()->active_display_list();
  }

  display::Display GetFirstDisplay() {
    DCHECK(!GetDisplays().empty());
    return GetDisplays().front();
  }

  display::Display CreateSingleDisplay() {
    UpdateDisplay("1111x111");
    return GetFirstDisplay();
  }
  void CreateMultipleDisplays() { UpdateDisplay("1111x111,2222x222,3333x333"); }

  void ResizeDisplay(const display::Display& display,
                     gfx::Size new_resolution) {
    // display::DisplayManagerTestApi offers a `SetDisplayResolution()` method,
    // but that does not seem to work (it does not relay the new resolution to
    // root window associated with the display).
    // So instead of using that method, we simply call `UpdateDisplay`.
    CHECK_EQ(GetDisplays().size(), 1u)
        << "This method only support single display setups!";

    UpdateDisplay(new_resolution.ToString());

    // Sanity check to ensure UpdateDisplay() didn't remote the existing display
    CHECK_EQ(GetFirstDisplay().id(), display.id());
  }

  DisplayId RemoveAllButFirstDisplay() {
    CHECK_GT(GetDisplays().size(), 1u)
        << "This method only works in multi display setups!";

    // UpdateDisplay() will reuse the existing display ids, so by calling it
    // with only a single display, all but the first display will be deleted.
    DisplayId last_display_id = GetDisplays().back().id();
    UpdateDisplay("1111x111");

    CHECK(!display_manager()->IsActiveDisplayId(last_display_id));
    return last_display_id;
  }

  bool IsAudioOutputMuted() {
    return CrasAudioHandler::Get()->IsOutputMutedBySecurityCurtain() &&
           CrasAudioHandler::Get()->IsOutputMuted();
  }

  bool IsAudioInputMuted() {
    return CrasAudioHandler::Get()->IsInputMutedBySecurityCurtain() &&
           CrasAudioHandler::Get()->IsInputMuted();
  }

  views::Widget& GetCurtainWidget() {
    return Shell::GetPrimaryRootWindowController()
        ->security_curtain_widget_controller()
        ->GetWidget();
  }

  views::Widget& GetOpenPowerWidget() {
    EXPECT_TRUE(power_button_test_api().IsMenuOpened());
    return *power_button_test_api().GetPowerButtonMenuView()->GetWidget();
  }

  const aura::Window& GetPowerMenuWidgetContainerParent() {
    EXPECT_TRUE(power_button_test_api().IsMenuOpened());

    return CHECK_DEREF(Shell::GetPrimaryRootWindow()
                           ->GetChildById(kShellWindowId_PowerMenuContainer)
                           ->parent());
  }

 private:
  std::unique_ptr<PowerButtonControllerTestApi> power_button_test_api_;
};

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotBeEnabledBeforeEnableIsCalled) {
  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       NoCurtainsShouldBeCreatedBeforeEnableIsCalled) {
  CreateMultipleDisplays();

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false));
  }
}

TEST_F(SecurityCurtainControllerImplTest, ShouldBeEnabledWhenCallingEnable) {
  security_curtain_controller().Enable(init_params());

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotBeEnabledWhenCallingDisable) {
  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  EXPECT_THAT(security_curtain_controller().IsEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainsShouldBeCreatedWhenCallingEnable) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable(init_params());

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(true));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainsShouldBeDestroyedWhenCallingDisable) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false));
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainsShouldCoverTheEntireDisplay) {
  CreateMultipleDisplays();
  security_curtain_controller().Enable(init_params());

  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());
    const views::Widget& curtain = GetCurtainForDisplay(display);
    EXPECT_THAT(curtain.IsVisible(), Eq(true));
    EXPECT_THAT(curtain.GetWindowBoundsInScreen(), Eq(display.bounds()));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainShouldKeepCoveringTheEntireDisplayAfterResizing) {
  display::Display display = CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());

  const views::Widget& curtain = GetCurtainForDisplay(display);

  for (gfx::Size new_resolution :
       {gfx::Size(1000, 500), gfx::Size(2000, 1000)}) {
    ResizeDisplay(display, new_resolution);
    EXPECT_THAT(curtain.GetWindowBoundsInScreen().size(), Eq(new_resolution));
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldUseViewFactory) {
  // To test that the view factory is used we simply create views with a very
  // specific id, and check that the curtain views have this id.
  constexpr int kId = 1234567;

  CreateMultipleDisplays();

  security_curtain_controller().Enable(WithViewFactory(base::BindRepeating(
      []() { return views::Builder<views::View>().SetID(kId).Build(); })));

  for (auto display : GetDisplays()) {
    views::Widget& curtain = GetCurtainForDisplay(display);

    EXPECT_EQ(curtain.GetContentsView()->GetID(), kId);
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       UncurtainedContainerShouldKeepCoveringTheEntireDisplayAfterResizing) {
  // This test ensures the uncurtained container also has the correct size.
  // That's very important for Chrome Remote Desktop as it streams this
  // container, and if it has the wrong size the stream will fail.
  display::Display display = CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());

  const aura::Window* curtained_off_container = Shell::GetContainer(
      Shell::GetRootWindowForDisplayId(GetFirstDisplay().id()),
      kShellWindowId_ScreenAnimationContainer);

  EXPECT_THAT(curtained_off_container->bounds().size(),
              Eq(display.bounds().size()));

  for (gfx::Size new_resolution :
       {gfx::Size(1000, 500), gfx::Size(2000, 1000)}) {
    ResizeDisplay(display, new_resolution);
    EXPECT_THAT(curtained_off_container->bounds().size(), Eq(new_resolution));
  }
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldDisableInputDevices) {
  ASSERT_TRUE(input_controller().AreInputDevicesEnabled());

  auto params = init_params();
  params.disable_input_devices = true;
  security_curtain_controller().Enable(params);

  EXPECT_FALSE(input_controller().AreInputDevicesEnabled());
}

TEST_F(SecurityCurtainControllerImplTest, UncurtainShouldReenableInputDevices) {
  auto params = init_params();
  params.disable_input_devices = true;
  security_curtain_controller().Enable(params);
  security_curtain_controller().Disable();

  EXPECT_TRUE(input_controller().AreInputDevicesEnabled());
}

TEST_F(SecurityCurtainControllerImplTest,
       CurtainShouldNotDisableInputDevicesWhenRequested) {
  auto params = init_params();
  params.disable_input_devices = false;
  security_curtain_controller().Enable(params);

  EXPECT_TRUE(input_controller().AreInputDevicesEnabled());
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotOccludeOtherWindows) {
  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  other_window->TrackOcclusionState();
  ASSERT_THAT(other_window->GetOcclusionState(),
              Eq(aura::Window::OcclusionState::VISIBLE));

  security_curtain_controller().Enable(init_params());

  EXPECT_THAT(other_window->GetOcclusionState(),
              Ne(aura::Window::OcclusionState::OCCLUDED));
}

TEST_F(SecurityCurtainControllerImplTest, CurtainShouldNotStealFocus) {
  auto other_window = CreateTestWindow(gfx::Rect(100, 100));
  other_window->Focus();
  ASSERT_THAT(other_window->HasFocus(), Eq(true));

  security_curtain_controller().Enable(init_params());

  EXPECT_THAT(other_window->HasFocus(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest, ShouldAddCurtainToNewDisplays) {
  CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(true));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldRemoveCurtainFromRemovedDisplays) {
  CreateMultipleDisplays();

  security_curtain_controller().Enable(init_params());
  DisplayId removed_display_id = RemoveAllButFirstDisplay();

  EXPECT_THAT(IsCurtainShownOnDisplay(removed_display_id), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotAddCurtainToNewDisplaysAfterCallingDisable) {
  CreateSingleDisplay();

  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  CreateMultipleDisplays();
  for (auto display : GetDisplays()) {
    SCOPED_TRACE("Failure on display " + display.ToString());

    EXPECT_THAT(IsCurtainShownOnDisplay(display), Eq(false));
  }
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldMuteAudioOutputAfterRequestedDelayWhileCurtainIsEnabled) {
  CreateSingleDisplay();

  auto delay = base::Minutes(5);
  auto params = init_params();
  params.mute_audio_output_after = delay;
  security_curtain_controller().Enable(params);
  EXPECT_FALSE(IsAudioOutputMuted());

  task_environment()->FastForwardBy(delay - base::Seconds(10));
  EXPECT_FALSE(IsAudioOutputMuted());

  task_environment()->FastForwardBy(base::Seconds(10));
  EXPECT_TRUE(IsAudioOutputMuted());

  security_curtain_controller().Disable();
  EXPECT_FALSE(IsAudioOutputMuted());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldMuteAudioOutputWhileCurtainIsEnabled) {
  CreateSingleDisplay();

  auto params = init_params();
  params.mute_audio_output_after = base::TimeDelta();
  security_curtain_controller().Enable(params);
  task_environment()->RunUntilIdle();  // Audio is muted asynchronously.
  EXPECT_TRUE(IsAudioOutputMuted());

  security_curtain_controller().Disable();
  EXPECT_FALSE(IsAudioOutputMuted());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotMuteAudioOutputWhenItsNotRequested) {
  CreateSingleDisplay();

  auto params = init_params();
  params.mute_audio_output_after = base::TimeDelta::Max();
  security_curtain_controller().Enable(params);
  EXPECT_FALSE(IsAudioOutputMuted());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldMuteAudioInputMuteWhileCurtainIsEnabled) {
  CreateSingleDisplay();

  auto params = init_params();
  params.mute_audio_input = true;
  security_curtain_controller().Enable(params);
  EXPECT_TRUE(IsAudioInputMuted());

  security_curtain_controller().Disable();
  EXPECT_FALSE(IsAudioInputMuted());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotMuteAudioInputMuteWhenItsNotRequested) {
  CreateSingleDisplay();

  auto params = init_params();
  params.mute_audio_input = false;
  security_curtain_controller().Enable(params);
  EXPECT_FALSE(IsAudioInputMuted());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldDisableNativeMouseCursorWhenEnableIsCalled) {
  ASSERT_THAT(IsNativeCursorEnabled(), Eq(true));

  security_curtain_controller().Enable(init_params());

  ASSERT_THAT(IsNativeCursorEnabled(), Eq(false));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldReenableNativeMouseCursorWhenDisableIsCalled) {
  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();

  ASSERT_THAT(IsNativeCursorEnabled(), Eq(true));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldMovePowerMenuWidgetAboveSecurityCurtainWhenEnabled) {
  security_curtain_controller().Enable(init_params());
  PressPowerButton();

  views::Widget& curtain_widget = GetCurtainWidget();
  views::Widget& power_menu_widget = GetOpenPowerWidget();

  ASSERT_TRUE(power_button_test_api().IsMenuOpened());
  EXPECT_TRUE(views::test::WidgetTest::IsWindowStackedAbove(&power_menu_widget,
                                                            &curtain_widget));
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldResetParentOfPowerMenuWidgetWhenDisabled) {
  PressPowerButton();
  const aura::Window& parent_before_enabled =
      GetPowerMenuWidgetContainerParent();
  ReleasePowerButton();

  security_curtain_controller().Enable(init_params());
  security_curtain_controller().Disable();
  PressPowerButton();
  const aura::Window& parent_after_disabled =
      GetPowerMenuWidgetContainerParent();

  ASSERT_EQ(&parent_before_enabled, &parent_after_disabled);
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldDismissOpenPowerMenuWidgetWhenCurtainModeIsEnabled) {
  PressPowerButton();

  security_curtain_controller().Enable(init_params());

  EXPECT_FALSE(power_button_test_api().IsMenuOpened());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldDismissOpenPowerMenuWidgetWhenCurtainModeIsDisabled) {
  security_curtain_controller().Enable(init_params());
  PressPowerButton();
  security_curtain_controller().Disable();

  EXPECT_FALSE(power_button_test_api().IsMenuOpened());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldForceDisableCameraAccessWhenCurtainModeIsEnabled) {
  auto params = init_params();
  params.disable_camera_access = true;
  security_curtain_controller().Enable(params);
  EXPECT_TRUE(camera_controller().IsCameraAccessForceDisabled());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldStopForceDisablingCameraAccessWhenCurtainModeIsDisabled) {
  auto params = init_params();
  params.disable_camera_access = true;
  security_curtain_controller().Enable(params);
  security_curtain_controller().Disable();

  EXPECT_FALSE(camera_controller().IsCameraAccessForceDisabled());
}

TEST_F(SecurityCurtainControllerImplTest,
       ShouldNotForceDisableCameraAccessWhenNotRequested) {
  auto params = init_params();
  params.disable_camera_access = false;
  security_curtain_controller().Enable(params);
  EXPECT_FALSE(camera_controller().IsCameraAccessForceDisabled());
}

}  // namespace ash::curtain
