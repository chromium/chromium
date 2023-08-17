// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/tab_slider.h"
#include "ash/style/tab_slider_button.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/bubble/bubble_view_ids.h"
#include "ash/system/video_conference/bubble/set_value_effects_view.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "media/capture/video/chromeos/mojom/cros_camera_service.mojom-shared.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"

namespace ash {

class CameraEffectsControllerTest : public NoSessionAshTestBase {
 public:
  // NoSessionAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVideoConference,
         features::kCameraEffectsSupportedByHardware},
        {});

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    AshTestBase::SetUp();

    camera_effects_controller_ = Shell::Get()->camera_effects_controller();

    // Enable test mode to mock the SetCameraEffects calls.
    camera_effects_controller_->bypass_set_camera_effects_for_testing(true);
  }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    controller_.reset();
  }

  // Sets background blur state.
  void SetBackgroundBlurEffectState(absl::optional<int> state) {
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kBackgroundBlur, state);
  }

  // Gets the state of the background blur effect from the effect's host,
  // `camera_effects_controller_`.
  int GetBackgroundBlurEffectState() {
    absl::optional<int> effect_state =
        camera_effects_controller_->GetEffectState(VcEffectId::kBackgroundBlur);
    DCHECK(effect_state.has_value());
    return effect_state.value();
  }

  // Retrieves the value of `prefs::kBackgroundBlur`.
  int GetBackgroundBlurPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetInteger(prefs::kBackgroundBlur);
  }

  // Simulates toggling portrait relighting effect state. Note that the `state`
  // argument doesn't matter for toggle effects.
  void TogglePortraitRelightingEffectState() {
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kPortraitRelighting, /*state=*/absl::nullopt);
  }

  // Simulates toggling camera framing effect state. Note that the `state`
  // argument doesn't matter for toggle effects.
  void ToggleCameraFramingEffectState() {
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kCameraFraming, /*state=*/absl::nullopt);
  }

  // Gets the state of the portrait relighting effect from the effect's host,
  // `camera_effects_controller_`.
  bool GetPortraitRelightingEffectState() {
    absl::optional<int> effect_state =
        camera_effects_controller_->GetEffectState(
            VcEffectId::kPortraitRelighting);
    DCHECK(effect_state.has_value());
    return static_cast<bool>(effect_state.value());
  }

  // Retrieves the value of `prefs::kPortraitRelighting`.
  bool GetPortraitRelightingPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetBoolean(prefs::kPortraitRelighting);
  }

  void SetAutozoomSupportState(bool autozoom_supported) {
    auto* autozoom_controller = Shell::Get()->autozoom_controller();

    autozoom_controller->SetAutozoomSupported(autozoom_supported);

    if (!autozoom_supported) {
      return;
    }

    // Autozoom is only supported if there's an active camera client, so we
    // simulate that here.
    autozoom_controller->OnActiveClientChange(
        cros::mojom::CameraClientType::ASH_CHROME,
        /*is_new_active_client=*/true,
        /*active_device_ids=*/{"fake_id"});
  }

  CameraEffectsController* camera_effects_controller() {
    return camera_effects_controller_;
  }

  FakeVideoConferenceTrayController* controller() { return controller_.get(); }

 protected:
  raw_ptr<CameraEffectsController, DanglingUntriaged | ExperimentalAsh>
      camera_effects_controller_ = nullptr;
  std::unique_ptr<FakeVideoConferenceTrayController> controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CameraEffectsControllerTest, IsEffectControlAvailable) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({}, {features::kVideoConference});
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({features::kVideoConference}, {});
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({features::kVideoConference},
                                         {features::kVcPortraitRelight});
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurOnEffectControlActivated) {
  SimulateUserLogin("testuser@gmail.com");

  // Activate the possible values of
  // `CameraEffectsController::BackgroundBlurPrefValue`, verify that the pref
  //  and internal state are all set properly.
  for (const auto state :
       {CameraEffectsController::BackgroundBlurPrefValue::kOff,
        CameraEffectsController::BackgroundBlurPrefValue::kLowest,
        CameraEffectsController::BackgroundBlurPrefValue::kLight,
        CameraEffectsController::BackgroundBlurPrefValue::kMedium,
        CameraEffectsController::BackgroundBlurPrefValue::kHeavy,
        CameraEffectsController::BackgroundBlurPrefValue::kMaximum}) {
    SetBackgroundBlurEffectState(state);
    EXPECT_EQ(GetBackgroundBlurPref(), state);
    EXPECT_EQ(GetBackgroundBlurEffectState(), state);
  }

  // Invalid background blur effect state should set the state to kOff.
  SetBackgroundBlurEffectState(
      static_cast<int>(
          CameraEffectsController::BackgroundBlurPrefValue::kMaximum) +
      1);
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);

  // Set the background blur state to be kMaximum.
  SetBackgroundBlurEffectState(
      CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  // Setting the background blur state to null will reset the effects as
  // kOff.
  SetBackgroundBlurEffectState(absl::nullopt);
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
}

TEST_F(CameraEffectsControllerTest,
       PortraitRelightingOnEffectControlActivated) {
  SimulateUserLogin("testuser@gmail.com");

  // Initial state should be "off".
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // Activating the effect should toggle it to "true."
  TogglePortraitRelightingEffectState();
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Another toggle should set it to "false."
  TogglePortraitRelightingEffectState();
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // And one more toggle should set it back to "true."
  TogglePortraitRelightingEffectState();
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());
}

TEST_F(CameraEffectsControllerTest, PrefOnCameraEffectChanged) {
  SimulateUserLogin("testuser@gmail.com");

  // Initial state should be "off".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // Case 1: when observe effects change from `CameraHalDispatcherImp`, the pref
  // is updated.
  cros::mojom::EffectsConfigPtr new_effects = cros::mojom::EffectsConfig::New();
  new_effects->blur_enabled = true;
  new_effects->blur_level = cros::mojom::BlurLevel::kMaximum;
  new_effects->relight_enabled = true;
  camera_effects_controller_->OnCameraEffectChanged(std::move(new_effects));

  // State should be "on".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Case 2: when new effects is null, the pref is unchanged.
  new_effects = cros::mojom::EffectsConfigPtr();
  camera_effects_controller_->OnCameraEffectChanged(std::move(new_effects));

  // State should be "on".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kMaximum);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Case 3: when observe default effects from `CameraHalDispatcherImp`, the
  // pref should be back to default.
  new_effects = cros::mojom::EffectsConfig::New();
  camera_effects_controller_->OnCameraEffectChanged(std::move(new_effects));

  // State should be "off".
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurPrefValue::kOff);
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());
}

TEST_F(CameraEffectsControllerTest, ResourceDependencyFlags) {
  SimulateUserLogin("testuser@gmail.com");

  // Makes sure that all registered effects have the correct dependency flag.
  auto* background_blur =
      camera_effects_controller()->GetEffectById(VcEffectId::kBackgroundBlur);
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kCamera,
            background_blur->dependency_flags());

  auto* portrait_relight = camera_effects_controller()->GetEffectById(
      VcEffectId::kPortraitRelighting);
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kCamera,
            portrait_relight->dependency_flags());
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurEnums) {
  // This test makes sure that `BackgroundBlurState` and
  // `BackgroundBlurPrefValue` is in sync with each other.
  EXPECT_EQ(
      static_cast<int>(CameraEffectsController::BackgroundBlurState::kMaximum),
      CameraEffectsController::BackgroundBlurPrefValue::kMaximum + 1);
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurMetricsRecord) {
  base::HistogramTester histogram_tester;

  SimulateUserLogin("testuser@gmail.com");

  // Update media status to make the video conference tray visible.
  VideoConferenceMediaState state;
  state.has_media_app = true;
  state.has_camera_permission = true;
  state.has_microphone_permission = true;
  state.is_capturing_screen = true;
  controller()->UpdateWithMediaState(state);

  auto* vc_tray = StatusAreaWidgetTestHelper::GetStatusAreaWidget()
                      ->video_conference_tray();

  // Open the vc bubble.
  LeftClickOn(vc_tray->toggle_bubble_button());

  // The set-value effects panel should have only 1 child view, and that view
  // should be the slider associated to the background blur effect.
  auto* set_value_effects_view = vc_tray->GetBubbleView()->GetViewByID(
      video_conference::BubbleViewID::kSetValueEffectsView);

  ASSERT_EQ(1u, set_value_effects_view->children().size());

  auto* background_blur_slider =
      static_cast<video_conference::SetValueEffectSlider*>(
          set_value_effects_view->children()[0]);
  EXPECT_EQ(VcEffectId::kBackgroundBlur, background_blur_slider->effect_id());

  auto* tab_slider = background_blur_slider->tab_slider();
  auto* first_button = tab_slider->GetButtonAtIndex(0);

  // At first, the first button is selected, but there should not be any metrics
  // recorded for that state since it is the default state when opening the
  // bubble.
  EXPECT_TRUE(first_button->selected());
  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kOff, 0);

  // Switching states by clicking slider buttons should record metrics in the
  // associated bucket.
  LeftClickOn(tab_slider->GetButtonAtIndex(1));

  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kLight, 1);

  LeftClickOn(tab_slider->GetButtonAtIndex(2));

  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kMaximum, 1);

  LeftClickOn(first_button);

  histogram_tester.ExpectBucketCount(
      "Ash.VideoConferenceTray.BackgroundBlur.Click",
      CameraEffectsController::BackgroundBlurState::kOff, 1);
}

TEST_F(CameraEffectsControllerTest, CameraFramingSupportState) {
  SimulateUserLogin("testuser@gmail.com");

  // By default autozoom is not supported, so the effect is not added.
  EXPECT_FALSE(
      camera_effects_controller()->GetEffectById(VcEffectId::kCameraFraming));

  SetAutozoomSupportState(true);

  EXPECT_TRUE(
      camera_effects_controller()->GetEffectById(VcEffectId::kCameraFraming));

  SetAutozoomSupportState(false);

  EXPECT_FALSE(
      camera_effects_controller()->GetEffectById(VcEffectId::kCameraFraming));
}

TEST_F(CameraEffectsControllerTest, CameraFramingToggle) {
  SetAutozoomSupportState(true);

  SimulateUserLogin("testuser@gmail.com");

  ASSERT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::OFF);

  ToggleCameraFramingEffectState();
  EXPECT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::ON_SINGLE);

  ToggleCameraFramingEffectState();
  EXPECT_EQ(Shell::Get()->autozoom_controller()->GetState(),
            cros::mojom::CameraAutoFramingState::OFF);
}

}  // namespace ash
