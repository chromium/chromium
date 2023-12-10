// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_commands.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chromeos/ash/components/audio/cras_audio_handler.h"
#include "chromeos/ash/components/dbus/audio/audio_node.h"
#include "chromeos/ash/components/dbus/audio/fake_cras_audio_client.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {
namespace accelerators {

using AcceleratorCommandsTest = AshTestBase;

TEST_F(AcceleratorCommandsTest, ToggleMinimized) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state1 = WindowState::Get(window1.get());
  WindowState* window_state2 = WindowState::Get(window2.get());
  window_state1->Activate();
  window_state2->Activate();

  ToggleMinimized();
  EXPECT_TRUE(window_state2->IsMinimized());
  EXPECT_FALSE(window_state2->IsNormalStateType());
  EXPECT_TRUE(window_state1->IsActive());

  ToggleMinimized();
  EXPECT_TRUE(window_state1->IsMinimized());
  EXPECT_FALSE(window_state1->IsNormalStateType());
  EXPECT_FALSE(window_state1->IsActive());

  // Toggling minimize when there are no active windows should unminimize and
  // activate the last active window.
  ToggleMinimized();
  EXPECT_FALSE(window_state1->IsMinimized());
  EXPECT_TRUE(window_state1->IsNormalStateType());
  EXPECT_TRUE(window_state1->IsActive());
}

TEST_F(AcceleratorCommandsTest, ToggleMaximized) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Activate();

  // When not in fullscreen, accelerators::ToggleMaximized toggles Maximized.
  EXPECT_FALSE(window_state->IsMaximized());
  ToggleMaximized();
  EXPECT_TRUE(window_state->IsMaximized());
  ToggleMaximized();
  EXPECT_FALSE(window_state->IsMaximized());

  // When in fullscreen accelerators::ToggleMaximized gets out of fullscreen.
  EXPECT_FALSE(window_state->IsFullscreen());
  ToggleFullscreen();
  EXPECT_TRUE(window_state->IsFullscreen());
  ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_FALSE(window_state->IsMaximized());
  ToggleMaximized();
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_TRUE(window_state->IsMaximized());
}

TEST_F(AcceleratorCommandsTest, Unpin) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(5, 5, 20, 20)));
  WindowState* window_state1 = WindowState::Get(window1.get());
  window_state1->Activate();

  window_util::PinWindow(window1.get(), /* trusted */ false);
  EXPECT_TRUE(window_state1->IsPinned());

  UnpinWindow();
  EXPECT_FALSE(window_state1->IsPinned());
}

TEST_F(AcceleratorCommandsTest, CycleSwapPrimaryDisplay) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("800x600,800x600,800x600");

  display::DisplayIdList id_list =
      display_manager()->GetConnectedDisplayIdList();

  ShiftPrimaryDisplay();
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[1], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[2], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[0], primary_id);
}

TEST_F(AcceleratorCommandsTest, CycleMixedMirrorModeSwapPrimaryDisplay) {
  UpdateDisplay("300x400,400x500,500x600");
  display::DisplayIdList id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Turn on mixed mirror mode. (Mirror from the first display to the second
  // display)
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(id_list[1]);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, id_list[0], dst_ids);

  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);

  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  ShiftPrimaryDisplay();
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[2], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[0], primary_id);

  ShiftPrimaryDisplay();
  primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_EQ(id_list[2], primary_id);
}

class AcceleratorCommandsAudioTest : public AcceleratorCommandsTest {
 public:
  void SetUpAudioNode() {
    auto* client = FakeCrasAudioClient::Get();
    client->SetAudioNodesForTesting({NewAudioNode(false, "INTERNAL_SPEAKER")});
  }

 protected:
  AudioNode NewAudioNode(bool is_input, const std::string& type) {
    ++node_count_;
    const std::string name =
        base::StringPrintf("%s-%" PRIu64, type.c_str(), node_count_);
    return AudioNode(is_input, node_count_, true, node_count_, node_count_,
                     name, type, name, false, 0, 2, 0, 0);
  }

  unsigned long node_count_ = 0;
};

TEST_F(AcceleratorCommandsAudioTest, VolumeSetToZeroAndThenMute) {
  SetUpAudioNode();
  auto* audio_handler = CrasAudioHandler::Get();
  // Make sure that volume is 0 and enter mute state.
  audio_handler->SetOutputVolumePercent(0);
  audio_handler->SetOutputMute(true);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  // Unmute and increase volume one step.
  PressAndReleaseKey(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  EXPECT_FALSE(audio_handler->IsOutputMuted());
  EXPECT_GE(audio_handler->GetOutputVolumePercent(), 0);
  // Volume down, should decrease to zero and no mute.
  PressAndReleaseKey(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  EXPECT_FALSE(audio_handler->IsOutputMuted());
  // Volume down again, should decrease to zero and mute.
  PressAndReleaseKey(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 0);
  // Output node mute state will not change.
  EXPECT_FALSE(audio_handler->IsOutputMuted());
}

TEST_F(AcceleratorCommandsAudioTest, ChangeVolumeAfterMuted) {
  SetUpAudioNode();
  auto* audio_handler = CrasAudioHandler::Get();
  // Make sure that output node is in mute state.
  audio_handler->SetOutputVolumePercent(80);
  audio_handler->SetOutputMute(true);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 80);
  // Press the volume down key will decrease the volume but won't change the
  // muted state.
  PressAndReleaseKey(ui::VKEY_VOLUME_DOWN, ui::EF_NONE);
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  EXPECT_LE(audio_handler->GetOutputVolumePercent(), 80);
  // Volume up, should bring back the volume to its original level and unmute.
  PressAndReleaseKey(ui::VKEY_VOLUME_UP, ui::EF_NONE);
  EXPECT_EQ(audio_handler->GetOutputVolumePercent(), 80);
  EXPECT_FALSE(audio_handler->IsOutputMuted());
}

TEST_F(AcceleratorCommandsAudioTest, VolumeMuteToggle) {
  auto* audio_handler = CrasAudioHandler::Get();
  EXPECT_FALSE(audio_handler->IsOutputMuted());
  VolumeMuteToggle();
  EXPECT_TRUE(audio_handler->IsOutputMuted());
  VolumeMuteToggle();
  EXPECT_FALSE(audio_handler->IsOutputMuted());
}

}  // namespace accelerators
}  // namespace ash
