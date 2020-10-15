// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"

#include <algorithm>

#include "base/check_op.h"

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy() = default;

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy(
    const gfx::Rect& bounds,
    bool is_docked)
    : bounds_(bounds),
      hide_inspected_contents_(bounds_.IsEmpty() && !bounds_.x() &&
                               !bounds_.y()),
      is_docked_(is_docked) {}

void DevToolsContentsResizingStrategy::CopyFrom(
    const DevToolsContentsResizingStrategy& strategy) {
  bounds_ = strategy.bounds();
  hide_inspected_contents_ = strategy.hide_inspected_contents();
  is_docked_ = strategy.is_docked();
}

bool DevToolsContentsResizingStrategy::Equals(
    const DevToolsContentsResizingStrategy& strategy) {
  return bounds_ == strategy.bounds() &&
         hide_inspected_contents_ == strategy.hide_inspected_contents() &&
         is_docked_ == strategy.is_docked();
}

void ApplyDevToolsContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy,
    const gfx::Size& container_size,
    gfx::Rect* new_devtools_bounds,
    gfx::Rect* new_contents_bounds) {
  new_devtools_bounds->SetRect(
      0, 0, container_size.width(), container_size.height());

  const gfx::Rect& bounds = strategy.bounds();

  if (bounds.size().IsEmpty() && !strategy.hide_inspected_contents()) {
    new_contents_bounds->SetRect(
        0, 0, container_size.width(), container_size.height());
    return;
  }

  int left = std::min(bounds.x(), container_size.width());
  int top = std::min(bounds.y(), container_size.height());
  int width = std::min(bounds.width(), container_size.width() - left);
  int height = std::min(bounds.height(), container_size.height() - top);

  if (strategy.is_docked()) {
    // Devtools console requires at least 240 pixels when docked.
    // https://cs.chromium.org/chromium/src/third_party/blink/renderer/devtools/front_end/ui/InspectorView.js?l=38&rcl=f8763532a3fe4f7d028f4cb23f56b289efbb70c0
    constexpr int kDevtoolsMinWidth = 240;
    // If container_size.width() == bounds.width(), dev tools is docked at
    // the bottom, otherwise it's docked to the right or left.
    const int available_content_width = container_size.width() == bounds.width()
                                            ? bounds.width()
                                            : container_size.width() - width;
    DCHECK_GE(available_content_width, 0);
    if (available_content_width < kDevtoolsMinWidth) {
      const int width_adjustment = kDevtoolsMinWidth - available_content_width;
      DCHECK_GE(width, width_adjustment);
      width -= width_adjustment;
    }
  }
  new_contents_bounds->SetRect(left, top, width, height);
}
