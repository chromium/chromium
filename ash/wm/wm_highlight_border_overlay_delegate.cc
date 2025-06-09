// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/wm_highlight_border_overlay_delegate.h"

#include "ash/wm/window_state.h"

namespace ash {

WmHighlightBorderOverlayDelegate::WmHighlightBorderOverlayDelegate() = default;
WmHighlightBorderOverlayDelegate::~WmHighlightBorderOverlayDelegate() = default;

bool WmHighlightBorderOverlayDelegate::ShouldRoundHighlightBorderForWindow(
    const aura::Window* window) {
  return WindowState::Get(window)->ShouldWindowHaveRoundedCorners();
}

}  // namespace ash
