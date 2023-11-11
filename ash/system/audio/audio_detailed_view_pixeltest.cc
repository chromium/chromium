// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "ash/constants/ash_features.h"
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
#include "chromeos/constants/chromeos_features.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr uint64_t kInternalMicId = 10003;

// Pixel tests for the quick settings audio detailed view.
class AudioDetailedViewPixelTest : public AshTestBase {
 public:
  AudioDetailedViewPixelTest() {
    feature_list_.InitWithFeatures({chromeos::features::kJelly}, {});
  }

  // AshTestBase:
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override {
    return pixel_test::InitParams();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AudioDetailedViewPixelTest, Basics) {
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
      /*revision_number=*/11, detailed_view));
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
                                CrasAudioHandler::ACTIVATE_BY_USER);

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
      /*revision_number=*/5, detailed_view));
}

}  // namespace ash
