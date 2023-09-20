// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overlay_layout_manager.h"

#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using display::Screen;

namespace ash {

OverlayLayoutManager::OverlayLayoutManager(aura::Window* overlay_container)
    : overlay_container_(overlay_container) {
  DCHECK(overlay_container_);
}

OverlayLayoutManager::~OverlayLayoutManager() = default;

void OverlayLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display.id() !=
      Screen::GetScreen()->GetDisplayNearestWindow(overlay_container_).id()) {
    // The update wasn't for this container's display.
    return;
  }

  for (aura::Window* child : overlay_container_->children()) {
    WindowState* window_state = WindowState::Get(child);
    if (window_state->IsFullscreen()) {
      const DisplayMetricsChangedWMEvent event(
          display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
      window_state->OnWMEvent(&event);
    }
  }
}

}  // namespace ash
