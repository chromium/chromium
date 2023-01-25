// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/camera_effects_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"

namespace ash {
namespace {

// Fake CameraEffectsController Observer to record notifications.
class FakeCameraEffectsControllerObserver
    : public CameraEffectsController::Observer {
 public:
  void OnCameraEffectsChanged(
      cros::mojom::EffectsConfigPtr new_effects) override {
    last_effects_ = std::move(new_effects);
    num_of_calls_ += 1;
  }

  int num_of_calls() { return num_of_calls_; }
  cros::mojom::EffectsConfigPtr last_effects() { return last_effects_.Clone(); }

 private:
  int num_of_calls_ = 0;
  cros::mojom::EffectsConfigPtr last_effects_;
};

class CameraEffectsControllerTest : public NoSessionAshTestBase {
 public:
  // NoSessionAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVideoConference, features::kVcBackgroundReplace}, {});

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests).
    controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    set_create_global_cras_audio_handler(false);

    AshTestBase::SetUp();

    camera_effects_controller_ = Shell::Get()->camera_effects_controller();

    // Mock the SetCameraEffects calls.
    camera_effects_controller_->set_effect_result_for_testing(
        cros::mojom::SetEffectResult::kOk);
  }

  void TearDown() override {
    NoSessionAshTestBase::TearDown();
    controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

  // Sets background blur state.
  void SetBackgroundBlurEffectState(int state) {
    camera_effects_controller_->OnEffectControlActivated(
        static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur), state);
  }

  // Gets the state of the background blur effect from the effect's host,
  // `camera_effects_controller_`.
  int GetBackgroundBlurEffectState() {
    absl::optional<int> effect_state =
        camera_effects_controller_->GetEffectState(
            static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur));
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

  // Gets the state of the portrait relighting effect from the effect's host,
  // `camera_effects_controller_`.
  bool GetPortraitRelightingEffectState() {
    absl::optional<int> effect_state =
        camera_effects_controller_->GetEffectState(
            static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight));
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

TEST_F(CameraEffectsControllerTest,
       IsCameraEffectsSupportedShouldBeConsistentWithFlags) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({}, {features::kVideoConference});
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({features::kVideoConference}, {});
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kPortraitRelight));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({}, {features::kVcBackgroundReplace});
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures({features::kVcBackgroundReplace}, {});
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundReplace));
  }
}

TEST_F(CameraEffectsControllerTest, NotifyObserverTest) {
  SimulateUserLogin("testuser@gmail.com");

  FakeCameraEffectsControllerObserver observer;
  camera_effects_controller_->AddObserver(&observer);

  // Mock the case where setting the effect state encounters an error.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetBackgroundBlurEffectState(
      static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Observers should not be notified if the SetCameraEffects fails.
  EXPECT_EQ(observer.num_of_calls(), 0);

  // Now mock the case where setting the effect state succeeds.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetBackgroundBlurEffectState(
      static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Observers should be notified if the SetCameraEffects succeeds.
  EXPECT_EQ(observer.num_of_calls(), 1);

  // Observers should be notified with the new EffectsConfigPtr.
  cros::mojom::EffectsConfigPtr notifyed_effects = observer.last_effects();
  EXPECT_TRUE(notifyed_effects->blur_enabled);
  EXPECT_EQ(notifyed_effects->blur_level, cros::mojom::BlurLevel::kMaximum);

  camera_effects_controller_->RemoveObserver(&observer);
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurOnEffectControlActivated) {
  SimulateUserLogin("testuser@gmail.com");

  // Activate the possible values of
  // `CameraEffectsController::BackgroundBlurEffectState`, verify that the pref
  //  and internal state are all set properly.
  for (const auto state :
       {CameraEffectsController::BackgroundBlurEffectState::kOff,
        CameraEffectsController::BackgroundBlurEffectState::kLowest,
        CameraEffectsController::BackgroundBlurEffectState::kLight,
        CameraEffectsController::BackgroundBlurEffectState::kMedium,
        CameraEffectsController::BackgroundBlurEffectState::kHeavy,
        CameraEffectsController::BackgroundBlurEffectState::kMaximum}) {
    camera_effects_controller_->OnEffectControlActivated(
        static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur), state);
    EXPECT_EQ(GetBackgroundBlurPref(), state);
    EXPECT_EQ(GetBackgroundBlurEffectState(), state);
  }

  // Invalid background blur effect state should set the state to kOff.
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      static_cast<int>(
          CameraEffectsController::BackgroundBlurEffectState::kMaximum) +
          1);
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurEffectState::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurEffectState::kOff);

  // Set the background blur state to be kMaximum.
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kMaximum);
  // Setting the background blur state to null will reset the effects as
  // kOff.
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      absl::nullopt);
  EXPECT_EQ(GetBackgroundBlurPref(),
            CameraEffectsController::BackgroundBlurEffectState::kOff);
  EXPECT_EQ(GetBackgroundBlurEffectState(),
            CameraEffectsController::BackgroundBlurEffectState::kOff);

  // Now verify that each of the pref and effect state is set to kOff if any
  // error is encountered while setting the state.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  for (const auto state :
       {CameraEffectsController::BackgroundBlurEffectState::kOff,
        CameraEffectsController::BackgroundBlurEffectState::kLowest,
        CameraEffectsController::BackgroundBlurEffectState::kLight,
        CameraEffectsController::BackgroundBlurEffectState::kMedium,
        CameraEffectsController::BackgroundBlurEffectState::kHeavy,
        CameraEffectsController::BackgroundBlurEffectState::kMaximum}) {
    camera_effects_controller_->OnEffectControlActivated(
        static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur), state);
    EXPECT_EQ(GetBackgroundBlurPref(),
              CameraEffectsController::BackgroundBlurEffectState::kOff);
    EXPECT_EQ(GetBackgroundBlurEffectState(),
              CameraEffectsController::BackgroundBlurEffectState::kOff);
  }
}

TEST_F(CameraEffectsControllerTest,
       PortraitRelightingOnEffectControlActivated) {
  SimulateUserLogin("testuser@gmail.com");

  // Initial state should be "off".
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // Activating the effect should toggle it to "true." The `value` argument
  // doesn't matter for toggle effects.
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight),
      absl::nullopt);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Another toggle should set it to "false."
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight),
      absl::nullopt);
  EXPECT_FALSE(GetPortraitRelightingEffectState());
  EXPECT_FALSE(GetPortraitRelightingPref());

  // And one more toggle should set it back to "true."
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight),
      absl::nullopt);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());

  // Verify that the effect state and pref remain in the previous state (true)
  // if attempt encounters an error.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kPortraitRelight),
      absl::nullopt);
  EXPECT_TRUE(GetPortraitRelightingEffectState());
  EXPECT_TRUE(GetPortraitRelightingPref());
}

}  // namespace
}  // namespace ash
