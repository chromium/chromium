// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_LEGACY_IMPL_H_
#define ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_LEGACY_IMPL_H_

#include <memory>

#include "ash/assistant/ui/assistant_container_view_animator.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/animation_delegate_views.h"

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
class COMPONENT_EXPORT(ASSISTANT_UI) AssistantContainerViewAnimatorLegacyImpl
    : public AssistantContainerViewAnimator,
      public views::AnimationDelegateViews {
 public:
  AssistantContainerViewAnimatorLegacyImpl(
      AssistantViewDelegate* delegate,
      AssistantContainerView* assistant_container_view);

  ~AssistantContainerViewAnimatorLegacyImpl() override;

  // AssistantContainerViewAnimator:
  void Init() override;
  void OnBoundsChanged() override;
  void OnPreferredSizeChanged() override;
  void OnUiVisibilityChanged(
      AssistantVisibility new_visibility,
      AssistantVisibility old_visibility,
      base::Optional<AssistantEntryPoint> entry_point,
      base::Optional<AssistantExitPoint> exit_point) override;

  // views::AnimationDelegatViews:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

 private:
  void UpdateBackground();

  // Background/shadow.
  ui::Layer background_layer_;
  std::unique_ptr<views::BorderShadowLayerDelegate> shadow_delegate_;

  // Animation.
  std::unique_ptr<gfx::SlideAnimation> animation_;
  gfx::Size start_size_;
  gfx::Size end_size_;
  int start_radius_ = 0;
  int end_radius_ = 0;
  int start_frame_number_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewAnimatorLegacyImpl);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_ANIMATOR_LEGACY_IMPL_H_
