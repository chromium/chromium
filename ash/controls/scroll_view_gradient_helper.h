// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTROLS_SCROLL_VIEW_GRADIENT_HELPER_H_
#define ASH_CONTROLS_SCROLL_VIEW_GRADIENT_HELPER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/callback_list.h"
#include "ui/views/controls/scroll_view.h"

namespace ash {

class GradientLayerDelegate;

// Draws fade in / fade out gradients at the top and bottom of a ScrollView.
// Uses layer masks to draw the gradient. Each gradient only shows if the view
// can be scrolled in that direction. For efficiency, does not create a layer
// mask if no gradient is showing (i.e. if the scroll view contents fit in the
// viewport and hence the view cannot be scrolled).
//
// The gradient is created on the first call to UpdateGradientZone(), not in the
// constructor. This allows the helper to be created any time during view tree
// construction, even before the scroll view contents are known.
//
// Views using this helper should call UpdateGradientZone() whenever the scroll
// view bounds or contents bounds change (e.g. from Layout()).
//
// The gradient is destroyed when this object is destroyed. This does not add
// extra work in the common views::View teardown case (the layer would be
// destroyed anyway).
class ASH_EXPORT ScrollViewGradientHelper {
 public:
  // `scroll_view` must have a layer.
  explicit ScrollViewGradientHelper(views::ScrollView* scroll_view);
  ~ScrollViewGradientHelper();

  // Updates the gradients based on `scroll_view_` bounds and scroll position.
  void UpdateGradientZone();

  GradientLayerDelegate* gradient_layer_for_test() {
    return gradient_layer_.get();
  }

 private:
  friend class ScopedScrollViewGradientDisabler;

  // Removes the scroll view mask layer.
  void RemoveMaskLayer();

  // The scroll view being decorated.
  views::ScrollView* const scroll_view_;

  // Draws the fade in/out gradients via a `scroll_view_` mask layer.
  std::unique_ptr<GradientLayerDelegate> gradient_layer_;

  // Callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;
};

}  // namespace ash

#endif  // ASH_CONTROLS_SCROLL_VIEW_GRADIENT_HELPER_H_
