// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CHROMEVOX_MOCK_TOUCH_EXPLORATION_CONTROLLER_DELEGATE_H_
#define ASH_ACCESSIBILITY_CHROMEVOX_MOCK_TOUCH_EXPLORATION_CONTROLLER_DELEGATE_H_

#include "ash/accessibility/chromevox/touch_exploration_controller.h"
#include "ui/accessibility/ax_enums.mojom.h"

namespace ash {

class MockTouchExplorationControllerDelegate
    : public TouchExplorationControllerDelegate {
 public:
  MockTouchExplorationControllerDelegate();
  ~MockTouchExplorationControllerDelegate() override;

  // TouchExplorationControllerDelegate:
  void SetOutputLevel(int volume) override;
  void SilenceSpokenFeedback() override;
  void PlayVolumeAdjustEarcon() override;
  void PlayPassthroughEarcon() override;
  void PlayLongPressRightClickEarcon() override;
  void PlayEnterScreenEarcon() override;
  void PlayTouchTypeEarcon() override;
  void HandleAccessibilityGesture(ax::mojom::Gesture gesture,
                                  gfx::PointF location) override;

  const std::vector<float> VolumeChanges() const;
  size_t NumAdjustSounds() const;
  size_t NumPassthroughSounds() const;
  size_t NumLongPressRightClickSounds() const;
  size_t NumEnterScreenSounds() const;
  size_t NumTouchTypeSounds() const;
  ax::mojom::Gesture GetLastGesture() const;
  void ResetLastGesture();
  std::vector<gfx::Point>& GetTouchExplorePoints();

  void ResetCountersToZero();

 private:
  std::vector<float> volume_changes_;
  size_t num_times_adjust_sound_played_ = 0;
  size_t num_times_passthrough_played_ = 0;
  size_t num_times_long_press_right_click_played_ = 0;
  size_t num_times_enter_screen_played_ = 0;
  size_t num_times_touch_type_sound_played_ = 0;
  ax::mojom::Gesture last_gesture_ = ax::mojom::Gesture::kNone;
  std::vector<gfx::Point> touch_explore_points_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_CHROMEVOX_MOCK_TOUCH_EXPLORATION_CONTROLLER_DELEGATE_H_
