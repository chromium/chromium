// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"

#include <algorithm>

#include "ui/gfx/geometry/rect.h"

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy()
    : hide_inspected_contents_(false) {
}

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy(
    const gfx::Rect& bounds)
    : bounds_(bounds),
      hide_inspected_contents_(bounds_.IsEmpty() && !bounds_.x() &&
          !bounds_.y()) {
}


void DevToolsContentsResizingStrategy::CopyFrom(
    const DevToolsContentsResizingStrategy& strategy) {
  bounds_ = strategy.bounds();
  hide_inspected_contents_ = strategy.hide_inspected_contents();
}

bool DevToolsContentsResizingStrategy::Equals(
    const DevToolsContentsResizingStrategy& strategy) {
  return bounds_ == strategy.bounds() &&
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

  int left = std::min(bounds.x(), container_bounds.width());
  int top = std::min(bounds.y(), container_bounds.height());
  int width = std::min(bounds.width(), container_bounds.width());
  int height = std::min(bounds.height(), container_bounds.height());
  new_contents_bounds->SetRect(left + container_bounds.x(),
                               top + container_bounds.y(), width, height);
}
