// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERLAY_LAYOUT_MANAGER_H_
#define ASH_WM_OVERLAY_LAYOUT_MANAGER_H_

#include "ash/ash_export.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "base/macros.h"
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
  ~OverlayLayoutManager() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

 private:
  aura::Window* overlay_container_;

  DISALLOW_COPY_AND_ASSIGN(OverlayLayoutManager);
};

}  // namespace ash

#endif  // ASH_WM_OVERLAY_LAYOUT_MANAGER_H_
