// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"

using devtools::DockSide;

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy()
    : hide_inspected_contents_(false) {}

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy(
    DockSide dock_side,
    const gfx::Rect& bounds)
    : dock_side_(dock_side),
      bounds_(bounds),
      hide_inspected_contents_(bounds_.IsEmpty() && !bounds_.x() &&
                               !bounds_.y()) {}

void DevToolsContentsResizingStrategy::CopyFrom(
    const DevToolsContentsResizingStrategy& strategy) {
  dock_side_ = strategy.dock_side();
  bounds_ = strategy.bounds();
  hide_inspected_contents_ = strategy.hide_inspected_contents();
}

bool DevToolsContentsResizingStrategy::Equals(
    const DevToolsContentsResizingStrategy& strategy) {
  return dock_side_ == strategy.dock_side() && bounds_ == strategy.bounds() &&
         hide_inspected_contents_ == strategy.hide_inspected_contents();
}

void ApplyDevToolsContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy,
    const gfx::Rect& container_bounds,
    gfx::Rect* new_devtools_bounds,
    gfx::Rect* new_contents_bounds) {
  new_devtools_bounds->SetRect(container_bounds.x(), container_bounds.y(),
                               container_bounds.width(),
                               container_bounds.height());

  const gfx::Rect& bounds = strategy.bounds();
  if (bounds.size().IsEmpty() && !strategy.hide_inspected_contents()) {
    new_contents_bounds->SetRect(container_bounds.x(), container_bounds.y(),
                                 container_bounds.width(),
                                 container_bounds.height());
    return;
  }

  const int container_width = container_bounds.width();
  const int container_height = container_bounds.height();

  int left = std::min(bounds.x(), container_width);
  int top = std::min(bounds.y(), container_height);
  int width = std::min(bounds.width(), container_width);
  int height = std::min(bounds.height(), container_height);

  // Ensure DevTools gets at least minimum space when container bounds shrink.
  constexpr int kMinDevToolsWidth = 250;
  constexpr int kMinDevToolsHeight = 72;

  switch (strategy.dock_side()) {
    case DockSide::kRight: {
      const int max_contents_width =
          std::max(0, container_width - kMinDevToolsWidth);
      width = std::min(width, max_contents_width);
      break;
    }
    case DockSide::kBottom: {
      const int max_contents_height =
          std::max(0, container_height - kMinDevToolsHeight);
      height = std::min(height, max_contents_height);
      break;
    }
    case DockSide::kLeft:
    case DockSide::kNone:
      break;
  }

  new_contents_bounds->SetRect(left + container_bounds.x(),
                               top + container_bounds.y(), width, height);
}
