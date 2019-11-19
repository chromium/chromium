// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_ASSISTANT_OVERLAY_H_
#define ASH_SHELF_ASSISTANT_OVERLAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/view.h"

namespace ash {

class HomeButton;

class ASH_EXPORT AssistantOverlay : public views::View {
 public:
  explicit AssistantOverlay(HomeButton* host_view);
  ~AssistantOverlay() override;

  void StartAnimation(bool show_icon);
  void EndAnimation();
  void BurstAnimation();
  void HideAnimation();
  bool IsBursting() const {
    return AnimationState::BURSTING == animation_state_;
  }
  bool IsHidden() const { return AnimationState::HIDDEN == animation_state_; }

  // views::View:
  const char* GetClassName() const override;

 private:
  enum class AnimationState {
    // Indicates no animation is playing.
    HIDDEN = 0,
    // Indicates currently playing the starting animation.
    STARTING,
    // Indicates the current animation is in the bursting phase, which means no
    // turning back.
    BURSTING
  };

  std::unique_ptr<ui::Layer> ripple_layer_;

  HomeButton* host_view_;

  AnimationState animation_state_ = AnimationState::HIDDEN;

  // Whether showing the icon animation or not.
  bool show_icon_ = false;

  views::CircleLayerDelegate circle_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AssistantOverlay);
};

}  // namespace ash
#endif  // ASH_SHELF_ASSISTANT_OVERLAY_H_
