// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERLAY_LAYOUT_MANAGER_H_
#define ASH_WM_OVERLAY_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "base/memory/raw_ptr.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}

namespace ash {

// Updates the bounds of widgets in the overlay container whenever the display
// bounds change. Keeps children snapped to pixel bounds.
class ASH_EXPORT OverlayLayoutManager : public WmDefaultLayoutManager,
                                        public display::DisplayObserver {
 public:
  explicit OverlayLayoutManager(aura::Window* overlay_container);

  OverlayLayoutManager(const OverlayLayoutManager&) = delete;
  OverlayLayoutManager& operator=(const OverlayLayoutManager&) = delete;

  ~OverlayLayoutManager() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  raw_ptr<aura::Window> overlay_container_;

  display::ScopedDisplayObserver display_observer_{this};
};

}  // namespace ash

#endif  // ASH_WM_OVERLAY_LAYOUT_MANAGER_H_
