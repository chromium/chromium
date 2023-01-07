// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_UI_INFO_H_
#define ASH_PUBLIC_CPP_SHELF_UI_INFO_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

struct ASH_PUBLIC_EXPORT ScrollableShelfInfo {
  ScrollableShelfInfo();
  ScrollableShelfInfo(const ScrollableShelfInfo& info);
  ScrollableShelfInfo& operator=(const ScrollableShelfInfo& info);
  ~ScrollableShelfInfo();

  // Current offset on the main axis.
  float main_axis_offset = 0.f;

  // Offset required to scroll to the next shelf page.
  float page_offset = 0.f;

  // Target offset on the main axis.
  float target_main_axis_offset = 0.f;

  // Bounds of the left arrow in screen.
  gfx::Rect left_arrow_bounds;

  // Bounds of the right arrow in screen.
  gfx::Rect right_arrow_bounds;

  // Indicates whether scrollable shelf is under scroll animation.
  bool is_animating = false;

  // Indicates whether there is any shelf icon under bounds animation.
  bool icons_under_animation = false;

  // Indicates whether scrollable shelf is in overflow mode.
  bool is_overflow = false;

  // Screen bounds of visible shelf icons.
  std::vector<gfx::Rect> icons_bounds_in_screen;

  // Indicates whether shelf widget is animating;
  bool is_shelf_widget_animating = false;
};

struct ASH_PUBLIC_EXPORT ShelfState {
  // The distance by which shelf will scroll.
  float scroll_distance = 0.f;
};

struct ASH_PUBLIC_EXPORT HotseatSwipeDescriptor {
  // The start location of the swipe gesture in screen coordinates.
  gfx::Point swipe_start_location;

  // The end location of the swipe gesture in screen coordinates.
  gfx::Point swipe_end_location;
};

struct ASH_PUBLIC_EXPORT HotseatInfo {
  // The gesture to swipe the hotseat up from kHidden to kExtended. Note that
  // |swipe_up| is independent of the current hotseat state.
  HotseatSwipeDescriptor swipe_up;

  // Indicate whether the hotseat bounds are in animation.
  bool is_animating = false;

  // The current hotseat state.
  HotseatState hotseat_state = HotseatState::kHidden;

  // Indicates whether the hotseat is being autohidden.
  bool is_auto_hidden = false;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_UI_INFO_H_
