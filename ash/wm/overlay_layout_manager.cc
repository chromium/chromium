// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overlay_layout_manager.h"

#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

using display::Screen;

namespace ash {

OverlayLayoutManager::OverlayLayoutManager(aura::Window* overlay_container)
    : overlay_container_(overlay_container) {
  DCHECK(overlay_container_);
  Screen::GetScreen()->AddObserver(this);
}

OverlayLayoutManager::~OverlayLayoutManager() {
  Screen::GetScreen()->RemoveObserver(this);
}

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
      const WMEvent event(WM_EVENT_WORKAREA_BOUNDS_CHANGED);
      window_state->OnWMEvent(&event);
    }
  }
}

}  // namespace ash
