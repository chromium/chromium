// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/audio_effects_controller.h"

#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/video_conference/effects/video_conference_tray_effects_manager_types.h"
#include "ash/system/video_conference/fake_video_conference_tray_controller.h"
#include "ash/system/video_conference/video_conference_common.h"
#include "ash/system/video_conference/video_conference_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/cras_audio_client.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"

namespace ash {

namespace {

struct AudioNodeInfo {
  bool is_input;
  uint64_t id;
  const char* const device_name;
  const char* const type;
  const char* const name;
  uint32_t audio_effect;
};

constexpr uint32_t kNoiseCancellationAudioEffect = 1;
constexpr uint32_t kStyleTransferAudioEffect = 4;

constexpr AudioNodeInfo kInternalMic_NC[] = {
    {.is_input = true,
     .id = 10001,
     .device_name = "Internal Mic",
     .type = "INTERNAL_MIC",
     .name = "Internal Mic",
     .audio_effect = kNoiseCancellationAudioEffect}};

constexpr AudioNodeInfo kInternalMic_NoEffects[] = {
    {.is_input = true,
     .id = 10002,
     .device_name = "Internal Mic",
     .type = "INTERNAL_MIC",
     .name = "Internal Mic",
     .audio_effect = 0u}};

constexpr AudioNodeInfo kInternalMic_NC_ST[] = {
    {.is_input = true,
     .id = 10003,
     .device_name = "Internal Mic",
     .type = "INTERNAL_MIC",
     .name = "Internal Mic",
     .audio_effect =
         kNoiseCancellationAudioEffect | kStyleTransferAudioEffect}};

constexpr AudioNodeInfo kInternalMic_ST[] = {
    {.is_input = true,
     .id = 10004,
     .device_name = "Internal Mic",
     .type = "INTERNAL_MIC",
     .name = "Internal Mic",
     .audio_effect = kStyleTransferAudioEffect}};

constexpr AudioNodeInfo kInternalSpeakerWithNC[] = {
    {.is_input = false,
     .id = 20001,
     .device_name = "Internal Speaker",
     .type = "INTERNAL_SPEAKER",
     .name = "Internal Speaker",
     .audio_effect = kNoiseCancellationAudioEffect}};

constexpr AudioNodeInfo kInternalSpeakerWithoutNC[] = {
    {.is_input = false,
     .id = 20002,
     .device_name = "Internal Speaker",
     .type = "INTERNAL_SPEAKER",
     .name = "Internal Speaker",
     .audio_effect = 0u}};

AudioNode GenerateAudioNode(const AudioNodeInfo* node_info) {
  return AudioNode(node_info->is_input, node_info->id,
                   /*has_v2_stable_device_id=*/false, node_info->id,
                   /*stable_device_id_v2=*/0, node_info->device_name,
                   node_info->type, node_info->name, /*active=*/false,
                   /* plugged_time=*/0,
                   /*max_supported_channels=*/1, node_info->audio_effect,
                   /*number_of_volume_steps=*/0);
}

AudioNodeList GenerateAudioNodeList(
    const std::vector<const AudioNodeInfo*>& nodes) {
  AudioNodeList node_list;
  for (auto* node_info : nodes) {
    node_list.emplace_back(GenerateAudioNode(node_info));
  }
  return node_list;
}

}  // namespace

class AudioEffectsControllerTest : public NoSessionAshTestBase {
 public:
  AudioEffectsControllerTest() = default;

  AudioEffectsControllerTest(const AudioEffectsControllerTest&) = delete;
  AudioEffectsControllerTest& operator=(const AudioEffectsControllerTest&) =
      delete;

  ~AudioEffectsControllerTest() override = default;

  // NoSessionAshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kFeatureManagementVideoConference);

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

  void ChangeAudioInput(const AudioNodeInfo* node_info) {
    const std::vector<const AudioNodeInfo*> audio_info_nodes = {node_info};

    fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
        GenerateAudioNodeList(audio_info_nodes));
    cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());
  }

  void ChangeAudioOutput(bool noise_cancellation_supported) {
    // Noise cancellation support state for output device is set in the platform
    // level (not in chrome level), then it will propagate that state through
    // `RequestNoiseCancellationSupported()`. Since we can't access platform
    // level, we will mimic that process like below for testing purpose.
    fake_cras_audio_client()->SetNoiseCancellationSupported(
        noise_cancellation_supported);
    cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

    cras_audio_handler()->SwitchToDevice(
        AudioDevice(GenerateAudioNode(noise_cancellation_supported
                                          ? kInternalSpeakerWithNC
                                          : kInternalSpeakerWithoutNC)),
        /*notify=*/true, DeviceActivateType::kActivateByUser);
  }

  VideoConferenceTray* GetVideoConfereneTray() {
    return StatusAreaWidgetTestHelper::GetStatusAreaWidget()
        ->video_conference_tray();
  }

  void OpenVideoConferenceBubble() {
    // Update media status to make the video conference tray visible.
    VideoConferenceMediaState state;
    state.has_media_app = true;
    state.has_camera_permission = true;
    state.has_microphone_permission = true;
    state.is_capturing_screen = true;
    tray_controller_->UpdateWithMediaState(state);

    // Open the bubble by clicking the toggle button.
    LeftClickOn(GetVideoConfereneTray()->toggle_bubble_button());
  }

  base::HistogramTester histogram_tester_;

 private:
  raw_ptr<AudioEffectsController, DanglingUntriaged> audio_effects_controller_ =
      nullptr;
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
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));
}

TEST_F(AudioEffectsControllerTest,
       NoiseCancellationNotSupportedByVcIfStyleTransferSupported) {
  // Change audio input to support both NC and style_transfer
  ChangeAudioInput(kInternalMic_NC_ST);

  // Prepare `CrasAudioHandler` to report that noise cancellation and style
  // transfer are both supported by hardware.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());
  cras_audio_handler()->RequestStyleTransferSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kStyleTransfer));
  EXPECT_TRUE(
      audio_effects_controller()->GetEffectById(VcEffectId::kStyleTransfer));
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSupported) {
  // Prepare `CrasAudioHandler` to report that noise cancellation is supported.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports that noise cancellation is supported.
  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_TRUE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  // Makes sure the dependency flag is set when the effect is supported.
  auto* effect =
      audio_effects_controller()->GetEffectById(VcEffectId::kNoiseCancellation);
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kMicrophone,
            effect->dependency_flags());

  // Delegate should be registered.
  EXPECT_TRUE(VideoConferenceTrayController::Get()
                  ->GetEffectsManager()
                  .IsDelegateRegistered(audio_effects_controller()));
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationNotEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(
      false, CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray, 1);

  // Noise cancellation effect state is disabled.
  std::optional<int> effect_state = audio_effects_controller()->GetEffectState(
      VcEffectId::kNoiseCancellation);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 0);

  cras_audio_handler()->SetNoiseCancellationState(
      true, CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);
  effect_state = audio_effects_controller()->GetEffectState(
      VcEffectId::kNoiseCancellation);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 1);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray, 2);
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(
      true, CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);
  histogram_tester_.ExpectBucketCount(
      CrasAudioHandler::kNoiseCancellationEnabledSourceHistogramName,
      CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray, 1);

  // Noise cancellation effect state is disabled.
  std::optional<int> effect_state = audio_effects_controller()->GetEffectState(
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
  cras_audio_handler()->SetNoiseCancellationState(
      true, CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);

  // Check that noise cancellation is enabled.
  EXPECT_TRUE(cras_audio_handler()->GetNoiseCancellationState());

  // User pressed the noise cancellation toggle.
  audio_effects_controller()->OnEffectControlActivated(
      VcEffectId::kNoiseCancellation, std::nullopt);

  // State should now be disabled.
  EXPECT_FALSE(cras_audio_handler()->GetNoiseCancellationState());
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSetEnabled) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable noise cancellation.
  cras_audio_handler()->SetNoiseCancellationState(
      false, CrasAudioHandler::AudioSettingsChangeSource::kVideoConferenceTray);

  // Check that noise cancellation is disabled.
  EXPECT_FALSE(cras_audio_handler()->GetNoiseCancellationState());

  // User pressed the noise cancellation toggle.
  audio_effects_controller()->OnEffectControlActivated(
      VcEffectId::kNoiseCancellation, std::nullopt);

  // State should now be enabled.
  EXPECT_TRUE(cras_audio_handler()->GetNoiseCancellationState());
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationAudioInputDevice) {
  // Prepare `CrasAudioHandler` to report that noise cancellation is supported.
  // However, the input audio does not support noise cancellation.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());
  ChangeAudioInput(kInternalMic_NoEffects);

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports noise that cancellation is not-supported.
  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  // Change to an input that does support. The state should reflect that.
  ChangeAudioInput(kInternalMic_NC);
  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_TRUE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSwitchInputDevice) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic_NC, kInternalMic_NoEffects}));

  // Prepare `CrasAudioHandler` to report that noise cancellation is supported.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Switch to use `kInternalMic_NoEffects`, `AudioEffectsController` reports
  // noise that cancellation is not-supported.
  cras_audio_handler()->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic_NoEffects)), /*notify=*/true,
      DeviceActivateType::kActivateByUser);

  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  // Switch to use `kInternalMic_NC`, `AudioEffectsController` reports noise
  // that cancellation is supported.
  cras_audio_handler()->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic_NC)), /*notify=*/true,
      DeviceActivateType::kActivateByUser);

  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_TRUE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  // Switch back to use `kInternalMic_NoEffects`, `AudioEffectsController`
  // reports noise that cancellation is not-supported.
  cras_audio_handler()->SwitchToDevice(
      AudioDevice(GenerateAudioNode(kInternalMic_NoEffects)), /*notify=*/true,
      DeviceActivateType::kActivateByUser);

  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));
}

TEST_F(AudioEffectsControllerTest, NoiseCancellationSwitchOutputDevice) {
  fake_cras_audio_client()->SetAudioNodesAndNotifyObserversForTesting(
      GenerateAudioNodeList({kInternalMic_NC, kInternalSpeakerWithNC,
                             kInternalSpeakerWithoutNC}));

  // Prepare `CrasAudioHandler` to report that noise cancellation is supported.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Switch output device to not support NC, `AudioEffectsController` reports
  // noise that cancellation is not-supported.
  ChangeAudioOutput(/*noise_cancellation_supported=*/false);

  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  // Switch output device to support NC, `AudioEffectsController` reports noise
  // that cancellation is supported.
  ChangeAudioOutput(/*noise_cancellation_supported=*/true);

  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_TRUE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));

  // Switch back output device to not support NC, `AudioEffectsController`
  // reports noise that cancellation is not-supported.
  ChangeAudioOutput(/*noise_cancellation_supported=*/false);

  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_FALSE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));
}

TEST_F(AudioEffectsControllerTest, CloseBubble) {
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());
  ChangeAudioInput(kInternalMic_NC);

  SimulateUserLogin("testuser1@gmail.com");

  OpenVideoConferenceBubble();
  ASSERT_TRUE(GetVideoConfereneTray()->GetBubbleView());

  // Change to an input device that does not support noise cancellation. The
  // bubble should close automatically to update effect state.
  ChangeAudioInput(kInternalMic_NoEffects);
  EXPECT_FALSE(GetVideoConfereneTray()->GetBubbleView());
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
      {features::kOnDeviceSpeechRecognition,
       features::kShowLiveCaptionInVideoConferenceTray},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Live caption feature flags are enabled, so `AudioEffectsController` reports
  // that live caption is supported.
  EXPECT_TRUE(
      audio_effects_controller()->IsEffectSupported(VcEffectId::kLiveCaption));
  EXPECT_TRUE(
      audio_effects_controller()->GetEffectById(VcEffectId::kLiveCaption));

  // Delegate should be registered.
  EXPECT_TRUE(VideoConferenceTrayController::Get()
                  ->GetEffectsManager()
                  .IsDelegateRegistered(audio_effects_controller()));
}

// Tests that with `features::kShowLiveCaptionInVideoConferenceTray`
// disabled, the live caption button does not show up in the vc tray.
TEST_F(AudioEffectsControllerTest, DoNotShowLiveCaptionInVcTray) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kOnDeviceSpeechRecognition},
      {features::kShowLiveCaptionInVideoConferenceTray});

  SimulateUserLogin("testuser1@gmail.com");

  EXPECT_FALSE(
      audio_effects_controller()->IsEffectSupported(VcEffectId::kLiveCaption));
  EXPECT_FALSE(
      audio_effects_controller()->GetEffectById(VcEffectId::kLiveCaption));
}

TEST_F(AudioEffectsControllerTest, LiveCaptionNotEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kOnDeviceSpeechRecognition},
                                       {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable live caption, confirm that it is disabled.
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(false);
  EXPECT_FALSE(controller->live_caption().enabled());

  // Live caption effect state is disabled.
  std::optional<int> state =
      audio_effects_controller()->GetEffectState(VcEffectId::kLiveCaption);
  EXPECT_TRUE(state.has_value());
  EXPECT_FALSE(state.value());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kOnDeviceSpeechRecognition},
                                       {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable live caption, confirm that it is enabled.
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(true);
  EXPECT_TRUE(controller->live_caption().enabled());

  // Live caption effect state is enabled.
  std::optional<int> state =
      audio_effects_controller()->GetEffectState(VcEffectId::kLiveCaption);
  EXPECT_TRUE(state.has_value());
  EXPECT_TRUE(state.value());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionSetNotEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kOnDeviceSpeechRecognition},
                                       {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly enable live caption, confirm that it is enabled.
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(true);
  EXPECT_TRUE(controller->live_caption().enabled());

  // User pressed the live caption toggle.
  audio_effects_controller()->OnEffectControlActivated(VcEffectId::kLiveCaption,
                                                       std::nullopt);

  // Live caption is now disabled.
  EXPECT_FALSE(controller->live_caption().enabled());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionSetEnabled) {
  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({features::kOnDeviceSpeechRecognition},
                                       {});

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable live caption, confirm that it is disabled.
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->live_caption().SetEnabled(false);
  EXPECT_FALSE(controller->live_caption().enabled());

  // User pressed the live caption toggle.
  audio_effects_controller()->OnEffectControlActivated(VcEffectId::kLiveCaption,
                                                       std::nullopt);

  // Live caption is now enabled.
  EXPECT_TRUE(controller->live_caption().enabled());
}

TEST_F(AudioEffectsControllerTest, LiveCaptionAndNoiseCancellationAdded) {
  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  // Ensure that live caption is supported.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      {features::kOnDeviceSpeechRecognition,
       features::kShowLiveCaptionInVideoConferenceTray},
      {});

  SimulateUserLogin("testuser1@gmail.com");

  // Both effects should be supported and added.
  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kNoiseCancellation));
  EXPECT_TRUE(
      audio_effects_controller()->IsEffectSupported(VcEffectId::kLiveCaption));

  EXPECT_TRUE(audio_effects_controller()->GetEffectById(
      VcEffectId::kNoiseCancellation));
  EXPECT_TRUE(
      audio_effects_controller()->GetEffectById(VcEffectId::kLiveCaption));

  // Delegate should be registered.
  EXPECT_TRUE(VideoConferenceTrayController::Get()
                  ->GetEffectsManager()
                  .IsDelegateRegistered(audio_effects_controller()));
}

TEST_F(AudioEffectsControllerTest, DelegateRegistered) {
  VideoConferenceTrayEffectsManager& effects_manager =
      VideoConferenceTrayController::Get()->GetEffectsManager();

  // No effects supported. Delegate should not be registered.
  SimulateUserLogin("testuser1@gmail.com");

  EXPECT_FALSE(
      effects_manager.IsDelegateRegistered(audio_effects_controller()));

  // Change audio input to support noise cancellation. Delegate should be
  // registered now.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());
  ChangeAudioInput(kInternalMic_NC);

  EXPECT_TRUE(effects_manager.IsDelegateRegistered(audio_effects_controller()));
}

TEST_F(AudioEffectsControllerTest, StyleTransferNotSupported) {
  // Change audio input to support both NC and style_transfer
  ChangeAudioInput(kInternalMic_NC_ST);

  // Prepare `CrasAudioHandler` to report that style transfer is
  // not-supported.
  fake_cras_audio_client()->SetStyleTransferSupported(false);
  cras_audio_handler()->RequestStyleTransferSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports that style transfer is not-supported.
  EXPECT_FALSE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kStyleTransfer));
  EXPECT_FALSE(
      audio_effects_controller()->GetEffectById(VcEffectId::kStyleTransfer));
}

TEST_F(AudioEffectsControllerTest, StyleTransferSupported) {
  // Change audio input to support both NC and style_transfer
  ChangeAudioInput(kInternalMic_NC_ST);

  // Prepare `CrasAudioHandler` to report that style transfer is supported.
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler()->RequestStyleTransferSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports thatstyle transfer is supported.
  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kStyleTransfer));
  EXPECT_TRUE(
      audio_effects_controller()->GetEffectById(VcEffectId::kStyleTransfer));

  // Makes sure the dependency flag is set when the effect is supported.
  auto* effect =
      audio_effects_controller()->GetEffectById(VcEffectId::kStyleTransfer);
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kMicrophone,
            effect->dependency_flags());

  // Delegate should be registered.
  EXPECT_TRUE(VideoConferenceTrayController::Get()
                  ->GetEffectsManager()
                  .IsDelegateRegistered(audio_effects_controller()));
}

TEST_F(AudioEffectsControllerTest, StyleTransferSupportedWithoutNC) {
  // Change audio input to support both NC and style_transfer
  ChangeAudioInput(kInternalMic_ST);

  // Prepare `CrasAudioHandler` to report that style transfer is supported.
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler()->RequestStyleTransferSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // `AudioEffectsController` reports thatstyle transfer is supported.
  EXPECT_TRUE(audio_effects_controller()->IsEffectSupported(
      VcEffectId::kStyleTransfer));
  EXPECT_TRUE(
      audio_effects_controller()->GetEffectById(VcEffectId::kStyleTransfer));

  // Makes sure the dependency flag is set when the effect is supported.
  auto* effect =
      audio_effects_controller()->GetEffectById(VcEffectId::kStyleTransfer);
  EXPECT_EQ(VcHostedEffect::ResourceDependency::kMicrophone,
            effect->dependency_flags());

  // Delegate should be registered.
  EXPECT_TRUE(VideoConferenceTrayController::Get()
                  ->GetEffectsManager()
                  .IsDelegateRegistered(audio_effects_controller()));
}

TEST_F(AudioEffectsControllerTest,
       StyleTransferEnableDisableFromCrasAudioHandler) {
  // Change audio input to support both NC and style_transfer
  ChangeAudioInput(kInternalMic_NC_ST);

  // Prepare `CrasAudioHandler` to report that style transfer is supported.
  fake_cras_audio_client()->SetStyleTransferSupported(true);
  cras_audio_handler()->RequestStyleTransferSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable style transfer.
  cras_audio_handler()->SetStyleTransferState(false);
  std::optional<int> effect_state =
      audio_effects_controller()->GetEffectState(VcEffectId::kStyleTransfer);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 0);

  // Explicitly enable style transfer.
  cras_audio_handler()->SetStyleTransferState(true);
  effect_state =
      audio_effects_controller()->GetEffectState(VcEffectId::kStyleTransfer);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 1);

  // Explicitly disable style transfer.
  cras_audio_handler()->SetStyleTransferState(false);
  effect_state =
      audio_effects_controller()->GetEffectState(VcEffectId::kStyleTransfer);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 0);
}

TEST_F(AudioEffectsControllerTest,
       StyleTransferEnableDisableFromAudioEffectsController) {
  // Change audio input to support both NC and style_transfer
  ChangeAudioInput(kInternalMic_NC_ST);

  // Prepare noise cancellation support.
  fake_cras_audio_client()->SetNoiseCancellationSupported(true);
  cras_audio_handler()->RequestNoiseCancellationSupported(base::DoNothing());

  SimulateUserLogin("testuser1@gmail.com");

  // Explicitly disable style transfer.
  cras_audio_handler()->SetStyleTransferState(false);
  std::optional<int> effect_state =
      audio_effects_controller()->GetEffectState(VcEffectId::kStyleTransfer);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 0);

  // User pressed the style transfer toggle.
  audio_effects_controller()->OnEffectControlActivated(
      VcEffectId::kStyleTransfer, std::nullopt);
  // CrasAudioHandler should return true.
  EXPECT_TRUE(cras_audio_handler()->GetStyleTransferState());
  // AudioEffectsController should return true.
  effect_state =
      audio_effects_controller()->GetEffectState(VcEffectId::kStyleTransfer);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 1);

  // User pressed the style transfer toggle.
  audio_effects_controller()->OnEffectControlActivated(
      VcEffectId::kStyleTransfer, std::nullopt);
  // CrasAudioHandler should return false.
  EXPECT_FALSE(cras_audio_handler()->GetStyleTransferState());
  // AudioEffectsController should return false.
  effect_state =
      audio_effects_controller()->GetEffectState(VcEffectId::kStyleTransfer);
  EXPECT_TRUE(effect_state.has_value());
  EXPECT_EQ(effect_state, 0);
}

}  // namespace ash
