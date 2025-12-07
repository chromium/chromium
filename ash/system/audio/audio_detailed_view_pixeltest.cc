// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>
#include <vector>

#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_helper.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler.h"
#include "chromeos/ash/components/audio/audio_devices_pref_handler_stub.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "third_party/cros_system_api/dbus/audio/dbus-constants.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"

namespace ash {

constexpr uint64_t kInternalMicId = 10003;

// Pixel tests for the quick settings audio detailed view.
class AudioDetailedViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple</*is_caption_enabled=*/bool,
                     /*enable_system_blur=*/bool>> {
 public:
  AudioDetailedViewPixelTest() {
    scoped_features_.InitWithFeatureState(features::kOnDeviceSpeechRecognition,
                                          IsCaptionEnabled());
  }

  bool IsCaptionEnabled() const { return std::get<0>(GetParam()); }
  bool IsSystemBlurEnabled() const { return std::get<1>(GetParam()); }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    pixel_test::InitParams init_params;
    init_params.system_blur_enabled = IsSystemBlurEnabled();
    return init_params;
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AudioDetailedViewPixelTest,
    testing::Combine(/*IsCaptionEnabled()=*/testing::Bool(),
                     /*IsSystemBlurEnabled()*/ testing::Bool()));

TEST_P(AudioDetailedViewPixelTest, Basics) {
  // Pin input and output devices to ensure consistent behavior.
  auto* audio_handler = CrasAudioHandler::Get();
  AudioDevice output_device(FakeCrasAudioClient::Get()->node_list()[1]);
  AudioDevice input_device(FakeCrasAudioClient::Get()->node_list()[5]);
  audio_handler->SwitchToDevice(output_device, true,
                                DeviceActivateType::kActivateByUser);
  audio_handler->SwitchToDevice(input_device, true,
                                DeviceActivateType::kActivateByUser);

  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowAudioDetailedView();

  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("qs_audio_detailed_view"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 17 : 0,
      detailed_view));
}

TEST_P(AudioDetailedViewPixelTest, ShowNoiseCancellationButton) {
  // Setup for showing noise cancellation button.
  auto* client = FakeCrasAudioClient::Get();
  auto* audio_handler = CrasAudioHandler::Get();
  auto internal_mic_node =
      AudioNode(true, kInternalMicId, false, kInternalMicId, 0, "Fake Mic",
                "INTERNAL_MIC", "Internal Mic", false /* is_active*/,
                0 /* pluged_time */, 1, cras::EFFECT_TYPE_NONE, 0);
  client->SetVoiceIsolationUIAppearance(
      VoiceIsolationUIAppearance(cras::EFFECT_TYPE_NOISE_CANCELLATION,
                                 cras::EFFECT_TYPE_NOISE_CANCELLATION, false));
  AudioNodeList node_list;
  node_list.push_back(internal_mic_node);
  client->SetAudioNodesAndNotifyObserversForTesting(node_list);
  client->SetNoiseCancellationSupported(true);
  scoped_refptr<AudioDevicesPrefHandlerStub> audio_pref_handler_ =
      base::MakeRefCounted<AudioDevicesPrefHandlerStub>();
  audio_pref_handler_->SetVoiceIsolationState(true);
  audio_handler->SetPrefHandlerForTesting(audio_pref_handler_);
  audio_handler->RequestNoiseCancellationSupported(base::DoNothing());
  audio_handler->RequestVoiceIsolationUIAppearance();
  audio_handler->SwitchToDevice(AudioDevice(internal_mic_node), true,
                                DeviceActivateType::kActivateByUser);

  UnifiedSystemTray* system_tray = GetPrimaryUnifiedSystemTray();
  system_tray->ShowBubble();
  ASSERT_TRUE(system_tray->bubble());

  system_tray->bubble()
      ->unified_system_tray_controller()
      ->ShowAudioDetailedView();

  TrayDetailedView* detailed_view =
      system_tray->bubble()
          ->quick_settings_view()
          ->GetDetailedViewForTest<TrayDetailedView>();
  ASSERT_TRUE(detailed_view);

  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      GenerateScreenshotName("qs_audio_detailed_view"),
      /*revision_number=*/pixel_test_helper()->IsSystemBlurEnabled() ? 9 : 0,
      detailed_view));
}

}  // namespace ash
