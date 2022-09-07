// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/touch_exploration_manager.h"

#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ash {

using TouchExplorationManagerTest = AshTestBase;

TEST_F(TouchExplorationManagerTest, AdjustSound) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  TouchExplorationManager touch_exploration_manager(controller);
  auto* audio_handler = CrasAudioHandler::Get();

  touch_exploration_manager.SetOutputLevel(10);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 10);
  EXPECT_FALSE(audio_handler->IsOutputMuted());

  touch_exploration_manager.SetOutputLevel(100);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 100);
  EXPECT_FALSE(audio_handler->IsOutputMuted());

  touch_exploration_manager.SetOutputLevel(0);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  EXPECT_TRUE(audio_handler->IsOutputMuted());

  touch_exploration_manager.SetOutputLevel(-10);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
}

TEST_F(TouchExplorationManagerTest, HandleAccessibilityGesture) {
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  TouchExplorationManager touch_exploration_manager(controller);
  TestAccessibilityControllerClient client;

  touch_exploration_manager.HandleAccessibilityGesture(
      ax::mojom::Gesture::kClick, gfx::PointF());
  EXPECT_EQ(ax::mojom::Gesture::kClick, client.last_a11y_gesture());

  touch_exploration_manager.HandleAccessibilityGesture(
      ax::mojom::Gesture::kSwipeLeft1, gfx::PointF());
  EXPECT_EQ(ax::mojom::Gesture::kSwipeLeft1, client.last_a11y_gesture());
}

}  //  namespace ash
