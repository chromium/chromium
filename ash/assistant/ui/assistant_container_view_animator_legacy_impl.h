// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_LEGACY_IMPL_H_
#define ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_LEGACY_IMPL_H_

#include <memory>

#include "ash/assistant/ui/assistant_container_view_animator.h"
#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace views {
class BorderShadowLayerDelegate;
}  // namespace views

namespace ash {

// The AssistantContainerViewAnimatorLegacyImpl is an implementation of
// AssistantContainerViewAnimator that performs rebounding of
// AssistantContainerView at each frame in its resize animation. As such, it is
// not very performant and we are working to deprecate this implementation.
class AssistantContainerViewAnimatorLegacyImpl
    : public AssistantContainerViewAnimator,
      public gfx::AnimationDelegate {
 public:
  AssistantContainerViewAnimatorLegacyImpl(
      AssistantController* assistant_controller,
      AssistantContainerView* assistant_container_view);

  ~AssistantContainerViewAnimatorLegacyImpl() override;

  // AssistantContainerViewAnimator:
  void Init() override;
  void OnBoundsChanged() override;
  void OnPreferredSizeChanged() override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  void UpdateBackground();

  // Background/shadow.
  ui::Layer background_layer_;
  std::unique_ptr<views::BorderShadowLayerDelegate> shadow_delegate_;

  // Animation.
  std::unique_ptr<gfx::SlideAnimation> animation_;
  gfx::SizeF start_size_;
  gfx::SizeF end_size_;
  int start_radius_ = 0;
  int end_radius_ = 0;
  int start_frame_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewAnimatorLegacyImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_LEGACY_IMPL_H_
