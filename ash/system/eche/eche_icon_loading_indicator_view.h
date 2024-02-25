// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ECHE_ECHE_ICON_LOADING_INDICATOR_VIEW_H_
#define ASH_SYSTEM_ECHE_ECHE_ICON_LOADING_INDICATOR_VIEW_H_

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace gfx {
class Canvas;
class Animation;
}  // namespace gfx

namespace ash {

// Draws a spinner around a given parent view.
class ASH_EXPORT EcheIconLoadingIndicatorView : public views::View,
                                                public views::ViewObserver,
                                                public gfx::AnimationDelegate {
  METADATA_HEADER(EcheIconLoadingIndicatorView, views::View)

 public:
  explicit EcheIconLoadingIndicatorView(views::View* parent);
  EcheIconLoadingIndicatorView(const EcheIconLoadingIndicatorView&) = delete;
  EcheIconLoadingIndicatorView& operator=(const EcheIconLoadingIndicatorView&) =
      delete;
  ~EcheIconLoadingIndicatorView() override;

  void SetAnimating(bool animating);
  bool GetAnimating() const;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  std::optional<base::TimeTicks> throbber_start_time_;

  raw_ptr<views::View> parent_ = nullptr;  // Unowned.

  base::ScopedObservation<views::View, views::ViewObserver> observed_session_{
      this};

  gfx::ThrobAnimation animation_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_ECHE_ECHE_ICON_LOADING_INDICATOR_VIEW_H_
