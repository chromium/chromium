// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_LOADING_INDICATOR_VIEW_H_
#define ASH_SYSTEM_PHONEHUB_LOADING_INDICATOR_VIEW_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace gfx {

class Canvas;
class Animation;

}  // namespace gfx

namespace ash {

// A view in Phone Hub that shows a spinner to indicate loading. This is drawn
// over the given parent view.
class ASH_EXPORT LoadingIndicatorView : public views::View,
                                        public views::ViewObserver,
                                        public gfx::AnimationDelegate {
 public:
  explicit LoadingIndicatorView(views::View* parent);
  LoadingIndicatorView(const LoadingIndicatorView&) = delete;
  LoadingIndicatorView& operator=(const LoadingIndicatorView&) = delete;
  ~LoadingIndicatorView() override;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

  void SetAnimating(bool animating);
  bool GetAnimating() const;

 private:
  absl::optional<base::TimeTicks> throbber_start_time_;

  views::View* parent_ = nullptr;  // Unowned.

  base::ScopedObservation<views::View, views::ViewObserver> observed_session_{
      this};

  gfx::ThrobAnimation animation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_LOADING_INDICATOR_VIEW_H_
