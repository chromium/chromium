// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_EXPAND_ARROW_VIEW_H_
#define ASH_APP_LIST_VIEWS_EXPAND_ARROW_VIEW_H_

#include <memory>

#include "ash/app_list/app_list_export.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class SlideAnimation;
}  // namespace gfx

namespace views {
class InkDrop;
class InkDropMask;
class InkDropRipple;
}  // namespace views

namespace ash {

class AppListView;
class ContentsView;

// A tile item for the expand arrow on the start page.
class APP_LIST_EXPORT ExpandArrowView : public views::Button,
                                        public views::ButtonListener,
                                        public views::ViewTargeterDelegate {
 public:
  ExpandArrowView(ContentsView* contents_view, AppListView* app_list_view);
  ~ExpandArrowView() override;

  // Overridden from views::Button:
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;
  const char* GetClassName() const override;

  // Overridden from views::InkDropHost:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override;
  std::unique_ptr<views::InkDropMask> CreateInkDropMask() const override;
  std::unique_ptr<views::InkDropRipple> CreateInkDropRipple() const override;

  void MaybeEnableHintingAnimation(bool enabled);

  bool IsHintingAnimationRunningForTest() {
    return hinting_animation_timer_.IsRunning();
  }

 private:
  // gfx::AnimationDelegate overrides:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  void TransitToFullscreenAllAppsState();

  // Schedule a hinting animation. |is_first_time| indicates whether the
  // animation is scheduled for the first time.
  void ScheduleHintingAnimation(bool is_first_time);
  void StartHintingAnimation();
  void ResetHintingAnimation();

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  ContentsView* const contents_view_;
  AppListView* const app_list_view_;  // Owned by the views hierarchy.

  // Properties for pulse opacity and size used in animation.
  float pulse_opacity_;
  int pulse_radius_;

  std::unique_ptr<gfx::SlideAnimation> animation_;

  // Whether the expand arrow view is pressed or not. If true, animation should
  // be canceled.
  bool button_pressed_ = false;

  // The y position offset of the arrow in this view.
  int arrow_y_offset_;

  base::OneShotTimer hinting_animation_timer_;

  base::WeakPtrFactory<ExpandArrowView> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExpandArrowView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_EXPAND_ARROW_VIEW_H_
