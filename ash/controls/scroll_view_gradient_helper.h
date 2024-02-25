// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTROLS_SCROLL_VIEW_GRADIENT_HELPER_H_
#define ASH_CONTROLS_SCROLL_VIEW_GRADIENT_HELPER_H_

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

// Draws fade in / fade out gradients at the top and bottom of a ScrollView.
// Uses shader based gradient set on the scroll view layer. Each gradient only
// shows if the view can be scrolled in that direction. For efficiency, does not
// create a gradient mask if no gradient is showing (i.e. if the scroll view
// contents fit in the viewport and hence the view cannot be scrolled).
//
// The gradient is created on the first call to UpdateGradientMask(), not in the
// constructor. This allows the helper to be created any time during view tree
// construction, even before the scroll view contents are known. This helper
// also controls the animation of the gradient view appearance, which is called
// only on the first call to UpdateGradientMak().
//
// Views using this helper should call UpdateGradientMask() whenever the scroll
// view bounds or contents bounds change (e.g. during layout).
//
// The gradient is destroyed when this object is destroyed. This does not add
// extra work in the common views::View teardown case (the layer would be
// destroyed anyway).
class ASH_EXPORT ScrollViewGradientHelper {
 public:
  // `scroll_view` must have a layer.
  ScrollViewGradientHelper(views::ScrollView* scroll_view, int gradient_height);
  ~ScrollViewGradientHelper();

  // Updates the gradients based on `scroll_view_` bounds and scroll position.
  // Draws the fade in/out gradients via a `scroll_view_` gradient mask.
  void UpdateGradientMask();

  const gfx::LinearGradient& gradient_mask_for_test() {
    return scroll_view_->layer()->gradient_mask();
  }

 private:
  friend class ScopedScrollViewGradientDisabler;

  // Sets the gradient mask animation for the layer.
  void AnimateMaskLayer(const gfx::LinearGradient& target_gradient);

  // Sets an empty gradient mask on the layer.
  void RemoveMaskLayer();

  // The scroll view being decorated.
  const raw_ptr<views::ScrollView> scroll_view_;

  // The height of the gradient in DIPs.
  const int gradient_height_;

  // Tracks the first call to UpdateGradientMask. Used to trigger animation only
  // on the first gradient mask update.
  bool first_time_update_{false};

  // Callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;
};

}  // namespace ash

#endif  // ASH_CONTROLS_SCROLL_VIEW_GRADIENT_HELPER_H_
