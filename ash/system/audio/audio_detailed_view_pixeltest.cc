// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/system/tray/tray_detailed_view.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_bubble.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/test/pixel/ash_pixel_test_init_params.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/view.h"

namespace ash {

constexpr uint64_t kInternalMicId = 10003;

// Pixel tests for the quick settings audio detailed view.
class AudioDetailedViewPixelTest : public AshTestBase {
 public:
  AudioDetailedViewPixelTest() {
    scoped_features_.InitWithFeatures({features::kOnDeviceSpeechRecognition},
                                      {});
  }

  // AshTestBase:
  std::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(AudioDetailedViewPixelTest, Basics) {
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
      "qs_audio_detailed_view",
      /*revision_number=*/14, detailed_view));
}

TEST_F(AudioDetailedViewPixelTest, ShowNoiseCancellationButton) {
  // Setup for showing noise cancellation button.
  auto* client = FakeCrasAudioClient::Get();
  auto* audio_handler = CrasAudioHandler::Get();
  auto internal_mic_node = AudioNode(
      true, kInternalMicId, false, kInternalMicId, 0, "Fake Mic",
      "INTERNAL_MIC", "Internal Mic", false /* is_active*/, 0 /* pluged_time */,
      1, cras::EFFECT_TYPE_NOISE_CANCELLATION, 0);
  AudioNodeList node_list;
  node_list.push_back(internal_mic_node);
  client->SetAudioNodesAndNotifyObserversForTesting(node_list);
  client->SetNoiseCancellationSupported(true);
  audio_handler->RequestNoiseCancellationSupported(base::DoNothing());
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
      "qs_audio_detailed_view",
      /*revision_number=*/6, detailed_view));
}

}  // namespace ash
