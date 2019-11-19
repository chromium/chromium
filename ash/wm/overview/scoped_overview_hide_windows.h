// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_HIDE_WINDOWS_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_HIDE_WINDOWS_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// ScopedOverviewHideWindows hides the list of windows in overview mode,
// remembers their visibility and recovers the visibility after overview mode.
class ASH_EXPORT ScopedOverviewHideWindows : public aura::WindowObserver {
 public:
  // |windows| the list of windows to hide in overview mode. If |force_hidden|
  // is true, the hidden windows may have their visibility altered during
  // overview, but we want to keep them hidden.
  ScopedOverviewHideWindows(const std::vector<aura::Window*>& windows,
                            bool force_hidden);
  ~ScopedOverviewHideWindows() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

 private:
  std::map<aura::Window*, bool> window_visibility_;
  bool force_hidden_;

  DISALLOW_COPY_AND_ASSIGN(ScopedOverviewHideWindows);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_HIDE_WINDOWS_H_
