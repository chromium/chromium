// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_HIGHLIGHT_BORDER_OVERLAY_DELEGATE_H_
#define ASH_WM_WM_HIGHLIGHT_BORDER_OVERLAY_DELEGATE_H_

#include "ash/ash_export.h"
#include "chromeos/ui/frame/highlight_border_overlay_delegate.h"

namespace ash {

class ASH_EXPORT WmHighlightBorderOverlayDelegate
    : public HighlightBorderOverlayDelegate {
 public:
  WmHighlightBorderOverlayDelegate();

  WmHighlightBorderOverlayDelegate(const WmHighlightBorderOverlayDelegate&) =
      delete;
  WmHighlightBorderOverlayDelegate& operator=(
      const WmHighlightBorderOverlayDelegate&) = delete;

  ~WmHighlightBorderOverlayDelegate() override;

  bool ShouldRoundHighlightBorderForWindow(const aura::Window* window) override;
};

}  // namespace ash

#endif  // ASH_WM_WM_HIGHLIGHT_BORDER_OVERLAY_DELEGATE_H_
