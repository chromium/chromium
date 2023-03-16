// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_effects_controller.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "media/base/media_switches.h"

namespace ash {

class AudioEffectsControllerTest : public NoSessionAshTestBase {
 public:
  AudioEffectsControllerTest() = default;

  AudioEffectsControllerTest(const AudioEffectsControllerTest&) = delete;
  AudioEffectsControllerTest& operator=(const AudioEffectsControllerTest&) =
      delete;

  ~AudioEffectsControllerTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kVideoConference}, {});
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kCameraEffectsSupportedByHardware);

    // Here we have to create the global instance of `CrasAudioHandler` before
    // `FakeVideoConferenceTrayController`, so we do it here and not in
    // `AshTestBase`.
    CrasAudioClient::InitializeFake();
    CrasAudioHandler::InitializeForTesting();

    // Instantiates a fake controller (the real one is created in
    // ChromeBrowserMainExtraPartsAsh::PreProfileInit() which is not called in
    // ash unit tests). This controller is needed because it owns the effects
    // manager.
    tray_controller_ = std::make_unique<FakeVideoConferenceTrayController>();

    set_create_global_cras_audio_handler(false);

    NoSessionAshTestBase::SetUp();

    audio_effects_controller_ = Shell::Get()->audio_effects_controller();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    tray_controller_.reset();
    CrasAudioHandler::Shutdown();
    CrasAudioClient::Shutdown();
  }

 protected:
  FakeCrasAudioClient* fake_cras_audio_client() {
    return FakeCrasAudioClient::Get();
  }

  CrasAudioHandler* cras_audio_handler() { return CrasAudioHandler::Get(); }

  AudioEffectsController* audio_effects_controller() {
    return audio_effects_controller_;
  }

 private:
  AudioEffectsController* audio_effects_controller_ = nullptr;
  std::unique_ptr<FakeVideoConferenceTrayController> tray_controller_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AudioEffectsControllerTest, NoiseCancellationNotSupported) {
  // Prepare `CrasAudioHandler` to report that noise cancellation is
  // not-supported.
  fake_cras_audio_client()->SetNoiseCancellationSupported(false);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports noise that cancellation is not-supported.
  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSupported) {
  // Prepare `CrasAudioHandler` to report that noise cancellation is supported.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports that noise cancellation is supported.
  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));

  // Makes sure the dependency flag is set when the effect is supported.
  auto* effect = audio_effects_controller()->GetEffect(0);
  ASSERT_EQ(VcEffectId::kNoiseCancellation, effect->id());
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kMicrophone,
            effect->dependency_flags());
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationNotEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(false);

  // Noise cancellation effect state is disabled.
  absl::optional<int> effect_state = audio_effects_controller()->GetEffectState(
      VcEffectId::kNoiseCancellation);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 0);

  cras_audio_handler()->SetNoiseCancellationState(true);
  effect_state = audio_effects_controller()->GetEffectState(
      VcEffectId::kNoiseCancellation);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 1);
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(true);

  // Noise cancellation effect state is disabled.
  absl::optional<int> effect_state = audio_effects_controller()->GetEffectState(
      VcEffectId::kNoiseCancellation);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 1);
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSetNotEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(true);

  // Check that noise cancellation is enabled.
  EXPECT_TRUE(cras_audio_handler()->GetNoiseCancellationState());

  // User pressed the noise cancellation toggle.
  audio_effects_controller()->OnEffectControlActivated(
      VcEffectId::kNoiseCancellation, absl::nullopt);

  // State should now be disabled.
  EXPECT_FALSE(cras_audio_handler()->GetNoiseCancellationState());
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSetEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(false);

  // Check that noise cancellation is disabled.
  EXPECT_FALSE(cras_audio_handler()->GetNoiseCancellationState());

  // User pressed the noise cancellation toggle.
  audio_effects_controller()->OnEffectControlActivated(
      VcEffectId::kNoiseCancellation, absl::nullopt);

  // State should now be enabled.
  EXPECT_TRUE(cras_audio_handler()->GetNoiseCancellationState());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionNotSupported) {
  SimulateUserLogin("testuser1@gmail.com");

  // No live caption feature flags enabled, so `AudioEffectsController` reports
  // that live caption is not supported.
  EXPECT_FALSE(
      audio_effects_controller()->IsEffectSupported(VcEffectId::kLiveCaption));
}

TEST_F(AudioEffectsControllerTest, LiveCaptionSupported) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
       ash::features::kOnDeviceSpeechRecognition},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Live caption feature flags are enabled, so `AudioEffectsController` reports
  // that live caption is supported.
  EXPECT_TRUE(
      audio_effects_controller()->IsEffectSupported(VcEffectId::kLiveCaption));
}

TEST_F(AudioEffectsControllerTest, LiveCaptionNotEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
       ash::features::kOnDeviceSpeechRecognition},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable live caption, confirm that it is disabled.
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(false);
  EXPECT_FALSE(controller->live_caption().enabled());

  // Live caption effect state is disabled.
  absl::optional<int> state =
      audio_effects_controller()->GetEffectState(VcEffectId::kLiveCaption);
  EXPECT_TRUE(state.has_value());
  EXPECT_FALSE(state.value());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
       ash::features::kOnDeviceSpeechRecognition},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable live caption, confirm that it is enabled.
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(true);
  EXPECT_TRUE(controller->live_caption().enabled());

  // Live caption effect state is enabled.
  absl::optional<int> state =
      audio_effects_controller()->GetEffectState(VcEffectId::kLiveCaption);
  EXPECT_TRUE(state.has_value());
  EXPECT_TRUE(state.value());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionSetNotEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
       ash::features::kOnDeviceSpeechRecognition},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable live caption, confirm that it is enabled.
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(true);
  EXPECT_TRUE(controller->live_caption().enabled());

  // User pressed the live caption toggle.
  audio_effects_controller()->OnEffectControlActivated(VcEffectId::kLiveCaption,
                                                       absl::nullopt);

  // Live caption is now disabled.
  EXPECT_FALSE(controller->live_caption().enabled());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionSetEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
       ash::features::kOnDeviceSpeechRecognition},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable live caption, confirm that it is disabled.
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(false);
  EXPECT_FALSE(controller->live_caption().enabled());

  // User pressed the live caption toggle.
  audio_effects_controller()->OnEffectControlActivated(VcEffectId::kLiveCaption,
                                                       absl::nullopt);

  // Live caption is now enabled.
  EXPECT_TRUE(controller->live_caption().enabled());
}

}  // namespace ash
