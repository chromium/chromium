// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "media/capture/video/chromeos/mojom/effects_pipeline.mojom.h"

namespace ash {
namespace {

class CameraEffectsControllerTest : public NoSessionAshTestBase {
 public:
  // NoSessionAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVideoConference, features::kVcBackgroundReplace}, {});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

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

  // Toggles portrait relighting state.
  void TogglePortraitRelightingEffectState() {
    // The `state` argument doesn't matter for toggle effects.
    camera_effects_controller_->OnEffectControlActivated(
        VcEffectId::kPortraitRelighting, /*state=*/absl::nullopt);
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

  CameraEffectsController* camera_effects_controller() {
    return camera_effects_controller_;
  }

 protected:
  CameraEffectsController* camera_effects_controller_ = nullptr;
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
  auto* background_blur = camera_effects_controller()->GetEffect(0);
  ASSERT_EQ(VcEffectId::kBackgroundBlur, background_blur->id());
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kCamera,
            background_blur->dependency_flags());

  auto* portrait_relight = camera_effects_controller()->GetEffect(1);
  ASSERT_EQ(VcEffectId::kPortraitRelighting, portrait_relight->id());
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

// TODO(b/274506848): Add unit test for background blur metrics record after the
// refactor.

}  // namespace
}  // namespace ash
