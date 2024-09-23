// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_ASSISTANT_OVERLAY_H_
#define ASH_SHELF_ASSISTANT_OVERLAY_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shelf/home_button.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"
#include "ui/views/view.h"

namespace ash {

class ASH_EXPORT AssistantOverlay : public views::View,
                                    public ui::ImplicitAnimationObserver {
  METADATA_HEADER(AssistantOverlay, views::View)

 public:
  explicit AssistantOverlay(HomeButton* host_view);

  AssistantOverlay(const AssistantOverlay&) = delete;
  AssistantOverlay& operator=(const AssistantOverlay&) = delete;

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
  void OnThemeChanged() override;

  // ui::ImplicitAnimationObserver
  void OnImplicitAnimationsCompleted() override;

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

  raw_ptr<HomeButton> host_view_;

  AnimationState animation_state_ = AnimationState::HIDDEN;

  // Whether showing the icon animation or not.
  bool show_icon_ = false;

  views::CircleLayerDelegate circle_layer_delegate_;
  std::unique_ptr<HomeButton::ScopedNoClipRect> scoped_no_clip_rect_;
};

}  // namespace ash
#endif  // ASH_SHELF_ASSISTANT_OVERLAY_H_
