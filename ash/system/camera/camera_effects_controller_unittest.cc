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
        {features::kVcControlsUi, features::kVCBackgroundBlur},
        {features::kVCBackgroundReplace, features::kVCPortraitRelighting});

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

  // Enables/Disables pref values.
  void SetEnabledPref(const std::string& perf_name, bool enabled) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetBoolean(
        perf_name, enabled);
  }

  // Updates prefs::kBackgroundBlur pref values.
  void SetBackgroundBlurPref(int level) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetInteger(
        prefs::kBackgroundBlur, level);
  }

  // Retrieves the value of `prefs::kBackgroundBlur`.
  int GetBackgroundBlurPref() {
    return Shell::Get()
        ->session_controller()
        ->GetActivePrefService()
        ->GetInteger(prefs::kBackgroundBlur);
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
    scoped_feature_list.InitWithFeatures(
        {}, {features::kVCBackgroundBlur, features::kVCBackgroundReplace,
             features::kVCPortraitRelighting});

    // No CameraEffects is supported if all flags are disabled.
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported());
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));

    // No camera effects supported and VC controls UI not enabled, no camera
    // effects UI controls available.
    EXPECT_TRUE(camera_effects_controller());
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable());
    EXPECT_FALSE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVcControlsUi, features::kVCBackgroundBlur},
        {features::kVCBackgroundReplace, features::kVCPortraitRelighting});

    // BackgroundBlur should be supported.
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported());
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));

    // Camera effects are supported, VC controls UI enabled, so camera effects
    // UI controls are available, and background blur UI is available.
    EXPECT_TRUE(camera_effects_controller());
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable());
    EXPECT_TRUE(camera_effects_controller()->IsEffectControlAvailable(
        cros::mojom::CameraEffect::kBackgroundBlur));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCBackgroundReplace},
        {features::kVCBackgroundBlur, features::kVCPortraitRelighting});

    // BackgroundReplace should be supported.
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported());
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCPortraitRelighting},
        {features::kVCBackgroundBlur, features::kVCBackgroundReplace});

    // PortraitRelight should be supported.
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported());
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_FALSE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCBackgroundBlur, features::kVCBackgroundReplace,
         features::kVCPortraitRelighting},
        {});

    // All camera effects should be supported.
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported());
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundBlur));
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kBackgroundReplace));
    EXPECT_TRUE(CameraEffectsController::IsCameraEffectsSupported(
        cros::mojom::CameraEffect::kPortraitRelight));
  }
}

TEST_F(CameraEffectsControllerTest,
       DefaultCameraEffectsShouldBeConsistentWithFlags) {
  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCBackgroundBlur},
        {features::kVCBackgroundReplace, features::kVCPortraitRelighting});

    SimulateUserLogin("testuser1@gmail.com");

    // BackgroundBlur should be enabled.
    cros::mojom::EffectsConfigPtr camera_effects =
        camera_effects_controller_->GetCameraEffects();
    EXPECT_TRUE(camera_effects->blur_enabled);
    EXPECT_FALSE(camera_effects->replace_enabled);
    EXPECT_FALSE(camera_effects->relight_enabled);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCBackgroundReplace},
        {features::kVCBackgroundBlur, features::kVCPortraitRelighting});

    SimulateUserLogin("testuser2@gmail.com");

    // BackgroundReplace should be enabled.
    cros::mojom::EffectsConfigPtr camera_effects =
        camera_effects_controller_->GetCameraEffects();
    EXPECT_FALSE(camera_effects->blur_enabled);
    EXPECT_TRUE(camera_effects->replace_enabled);
    EXPECT_FALSE(camera_effects->relight_enabled);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCPortraitRelighting},
        {features::kVCBackgroundBlur, features::kVCBackgroundReplace});

    SimulateUserLogin("testuser3@gmail.com");

    // PortraintRelighting should be enabled.
    cros::mojom::EffectsConfigPtr camera_effects =
        camera_effects_controller_->GetCameraEffects();
    EXPECT_FALSE(camera_effects->blur_enabled);
    EXPECT_FALSE(camera_effects->replace_enabled);
    EXPECT_TRUE(camera_effects->relight_enabled);
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {features::kVCBackgroundBlur, features::kVCBackgroundReplace,
         features::kVCPortraitRelighting},
        {});

    SimulateUserLogin("testuser4@gmail.com");

    // BackgroundReplace should be not enabled when all flags are enabled.
    cros::mojom::EffectsConfigPtr camera_effects =
        camera_effects_controller_->GetCameraEffects();
    EXPECT_TRUE(camera_effects->blur_enabled);
    EXPECT_FALSE(camera_effects->replace_enabled);
    EXPECT_TRUE(camera_effects->relight_enabled);
  }
}

TEST_F(CameraEffectsControllerTest, EnableBackgroundBlurTest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kVCBackgroundReplace},
      {features::kVCBackgroundBlur, features::kVCPortraitRelighting});

  SimulateUserLogin("testuser@gmail.com");

  // Check default pref values.
  cros::mojom::EffectsConfigPtr camera_effects =
      camera_effects_controller_->GetCameraEffects();
  EXPECT_FALSE(camera_effects->blur_enabled);
  EXPECT_TRUE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // Set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetBackgroundBlurPref(1);

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_FALSE(camera_effects->blur_enabled);
  EXPECT_TRUE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // Set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetBackgroundBlurPref(1);

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetBackgroundBlurPref(-1);

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetBackgroundBlurPref(-1);

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_FALSE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);
}

TEST_F(CameraEffectsControllerTest, EnableBackgroundReplaceTest) {
  SimulateUserLogin("testuser@gmail.com");

  cros::mojom::EffectsConfigPtr camera_effects =
      camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetEnabledPref(prefs::kBackgroundReplace, true);

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetEnabledPref(prefs::kBackgroundReplace, true);

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_FALSE(camera_effects->blur_enabled);
  EXPECT_TRUE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetEnabledPref(prefs::kBackgroundReplace, false);

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_FALSE(camera_effects->blur_enabled);
  EXPECT_TRUE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetEnabledPref(prefs::kBackgroundReplace, false);

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_FALSE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);
}

TEST_F(CameraEffectsControllerTest, EnablePortraitRelightingTest) {
  SimulateUserLogin("testuser@gmail.com");

  cros::mojom::EffectsConfigPtr camera_effects =
      camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetEnabledPref(prefs::kPortraitRelighting, true);

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);

  // set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetEnabledPref(prefs::kPortraitRelighting, true);

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_TRUE(camera_effects->relight_enabled);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetEnabledPref(prefs::kPortraitRelighting, false);

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_TRUE(camera_effects->relight_enabled);

  // set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetEnabledPref(prefs::kPortraitRelighting, false);

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_FALSE(camera_effects->replace_enabled);
  EXPECT_FALSE(camera_effects->relight_enabled);
}

TEST_F(CameraEffectsControllerTest, BlurLevelChangeTest) {
  SimulateUserLogin("testuser@gmail.com");

  cros::mojom::EffectsConfigPtr camera_effects =
      camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_EQ(camera_effects->blur_level, cros::mojom::BlurLevel::kLowest);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Pref values should be reverted.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_EQ(camera_effects->blur_level, cros::mojom::BlurLevel::kLowest);

  // set camera effect success should modify the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Pref values should be updated.
  camera_effects = camera_effects_controller_->GetCameraEffects();
  EXPECT_TRUE(camera_effects->blur_enabled);
  EXPECT_EQ(camera_effects->blur_level, cros::mojom::BlurLevel::kMaximum);
}

TEST_F(CameraEffectsControllerTest, NotifyObserverTest) {
  SimulateUserLogin("testuser@gmail.com");

  FakeCameraEffectsControllerObserver observer;
  camera_effects_controller_->AddObserver(&observer);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kError);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Observers should not be notified if the SetCameraEffects fails.
  EXPECT_EQ(observer.num_of_calls(), 0);

  // set camera effect error should revert the perf.
  camera_effects_controller_->set_effect_result_for_testing(
      cros::mojom::SetEffectResult::kOk);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Observers should be notified if the SetCameraEffects succeeds.
  EXPECT_EQ(observer.num_of_calls(), 1);

  // Observers should be notified with the new EffectsConfigPtr.
  cros::mojom::EffectsConfigPtr notifyed_effects = observer.last_effects();
  EXPECT_TRUE(notifyed_effects->blur_enabled);
  EXPECT_EQ(notifyed_effects->blur_level, cros::mojom::BlurLevel::kMaximum);

  camera_effects_controller_->RemoveObserver(&observer);
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurGetEffectState) {
  SimulateUserLogin("testuser@gmail.com");

  // Pref value is `kBackgroundBlurLevelForDisabling` (off).
  SetBackgroundBlurPref(-1);
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kOff);

  // Test all values of `cros::mojom::BlurLevel`.
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kLowest));
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kLowest);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kLight));
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kLight);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kMedium));
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kMedium);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kHeavy));
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kHeavy);
  SetBackgroundBlurPref(static_cast<int>(cros::mojom::BlurLevel::kMaximum));
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kMaximum);

  // Now verify with a pref value that isn't recognized as a valid background
  // blur state.
  SetBackgroundBlurPref(-999);
  EXPECT_EQ(camera_effects_controller_->GetEffectState(
                static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur)),
            CameraEffectsController::BackgroundBlurEffectState::kOff);
}

TEST_F(CameraEffectsControllerTest, BackgroundBlurOnEffectControlActivated) {
  SimulateUserLogin("testuser@gmail.com");

  // Activate `kOff`, verify that pref value is -1
  // (`kBackgroundBlurLevelForDisabling`).
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kOff);
  EXPECT_EQ(GetBackgroundBlurPref(), -1);

  // Activate the rest of the possible values of
  // `CameraEffectsController::BackgroundBlurEffectState`, verify that the pref
  // value is the expected value of `cros::mojom::BlurLevel`.
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kLowest);
  EXPECT_EQ(GetBackgroundBlurPref(),
            static_cast<int>(cros::mojom::BlurLevel::kLowest));
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kLight);
  EXPECT_EQ(GetBackgroundBlurPref(),
            static_cast<int>(cros::mojom::BlurLevel::kLight));
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kMedium);
  EXPECT_EQ(GetBackgroundBlurPref(),
            static_cast<int>(cros::mojom::BlurLevel::kMedium));
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kHeavy);
  EXPECT_EQ(GetBackgroundBlurPref(),
            static_cast<int>(cros::mojom::BlurLevel::kHeavy));
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur),
      CameraEffectsController::BackgroundBlurEffectState::kMaximum);
  EXPECT_EQ(GetBackgroundBlurPref(),
            static_cast<int>(cros::mojom::BlurLevel::kMaximum));

  // Passing an invalid background blur state is the same as activating
  // `kOff`.
  camera_effects_controller_->OnEffectControlActivated(
      static_cast<int>(cros::mojom::CameraEffect::kBackgroundBlur), -999);
  EXPECT_EQ(GetBackgroundBlurPref(), -1);
}

}  // namespace
}  // namespace ash
