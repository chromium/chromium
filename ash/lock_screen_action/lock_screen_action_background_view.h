// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_VIEW_H_
#define ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_VIEW_H_

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/animation/ink_drop_state.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// Widget delegate view that implements the contents of the lock action
// background widget.
// The widget hosts a view that contains a transparent view with a black ink
// drop. The view implementation provides methods to activate or hide the ink
// drop in the view.
class ASH_EXPORT LockScreenActionBackgroundView
    : public views::WidgetDelegateView,
      public views::InkDropObserver {
  METADATA_HEADER(LockScreenActionBackgroundView, views::WidgetDelegateView)

 public:
  LockScreenActionBackgroundView();

  LockScreenActionBackgroundView(const LockScreenActionBackgroundView&) =
      delete;
  LockScreenActionBackgroundView& operator=(
      const LockScreenActionBackgroundView&) = delete;

  ~LockScreenActionBackgroundView() override;

  // Request the ink drop to be activated.
  // |done| - called when the ink drop animation ends.
  void AnimateShow(base::OnceClosure done);

  // Requests the ink drop to be hidden.
  // |done| - called when the ink drop animation ends.
  void AnimateHide(base::OnceClosure done);

  // views::InkDropListener:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  // views::WidgetDelegateView:
  bool CanActivate() const override;

 private:
  friend class LockScreenActionBackgroundViewTestApi;
  class NoteBackground;

  // Gets background_ as a views::View*.
  views::View* GetBackgroundView();

  base::OnceClosure animation_end_callback_;
  views::InkDropState animating_to_state_;

  raw_ptr<NoteBackground> background_ = nullptr;
};

}  // namespace ash

#endif  // ASH_LOCK_SCREEN_ACTION_LOCK_SCREEN_ACTION_BACKGROUND_VIEW_H_
