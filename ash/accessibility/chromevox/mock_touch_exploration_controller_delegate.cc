// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/chromevox/mock_touch_exploration_controller_delegate.h"

namespace ash {

MockTouchExplorationControllerDelegate::
    MockTouchExplorationControllerDelegate() = default;
MockTouchExplorationControllerDelegate::
    ~MockTouchExplorationControllerDelegate() = default;

void MockTouchExplorationControllerDelegate::SetOutputLevel(int volume) {
  volume_changes_.push_back(volume);
}

void MockTouchExplorationControllerDelegate::SilenceSpokenFeedback() {}
void MockTouchExplorationControllerDelegate::PlayVolumeAdjustEarcon() {
  ++num_times_adjust_sound_played_;
}

void MockTouchExplorationControllerDelegate::PlayPassthroughEarcon() {
  ++num_times_passthrough_played_;
}

void MockTouchExplorationControllerDelegate::PlayLongPressRightClickEarcon() {
  ++num_times_long_press_right_click_played_;
}

void MockTouchExplorationControllerDelegate::PlayEnterScreenEarcon() {
  ++num_times_enter_screen_played_;
}

void MockTouchExplorationControllerDelegate::PlayTouchTypeEarcon() {
  ++num_times_touch_type_sound_played_;
}
void MockTouchExplorationControllerDelegate::HandleAccessibilityGesture(
    ax::mojom::Gesture gesture,
    gfx::PointF location) {
  last_gesture_ = gesture;
  if (gesture == ax::mojom::Gesture::kTouchExplore)
    touch_explore_points_.push_back(gfx::Point(location.x(), location.y()));
}

const std::vector<float> MockTouchExplorationControllerDelegate::VolumeChanges()
    const {
  return volume_changes_;
}

size_t MockTouchExplorationControllerDelegate::NumAdjustSounds() const {
  return num_times_adjust_sound_played_;
}

size_t MockTouchExplorationControllerDelegate::NumPassthroughSounds() const {
  return num_times_passthrough_played_;
}

size_t MockTouchExplorationControllerDelegate::NumLongPressRightClickSounds()
    const {
  return num_times_long_press_right_click_played_;
}

size_t MockTouchExplorationControllerDelegate::NumEnterScreenSounds() const {
  return num_times_enter_screen_played_;
}

size_t MockTouchExplorationControllerDelegate::NumTouchTypeSounds() const {
  return num_times_touch_type_sound_played_;
}

ax::mojom::Gesture MockTouchExplorationControllerDelegate::GetLastGesture()
    const {
  return last_gesture_;
}

void MockTouchExplorationControllerDelegate::ResetLastGesture() {
  last_gesture_ = ax::mojom::Gesture::kNone;
}

std::vector<gfx::Point>&
MockTouchExplorationControllerDelegate::GetTouchExplorePoints() {
  return touch_explore_points_;
}

void MockTouchExplorationControllerDelegate::ResetCountersToZero() {
  num_times_adjust_sound_played_ = 0;
  num_times_passthrough_played_ = 0;
  num_times_long_press_right_click_played_ = 0;
  num_times_enter_screen_played_ = 0;
  num_times_touch_type_sound_played_ = 0;
}

}  // namespace ash
