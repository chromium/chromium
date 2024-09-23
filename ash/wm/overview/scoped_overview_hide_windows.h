// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_HIDE_WINDOWS_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_HIDE_WINDOWS_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Manages window visibility in Overview mode by doing the following:
// 1. Hides windows in overview mode based on certain criteria.
// 2. Stores the original visibility state of windows before they are hidden.
// 3. Restores hidden windows to their previous visibility state when the
// conditions for hiding are no longer met.
class ScopedOverviewHideWindows : public aura::WindowObserver {
 public:
  // |windows| the list of windows to hide in overview mode. If |force_hidden|
  // is true, the hidden windows may have their visibility altered during
  // overview, but we want to keep them hidden.
  ScopedOverviewHideWindows(
      const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows,
      bool force_hidden);
  ScopedOverviewHideWindows(const ScopedOverviewHideWindows&) = delete;
  ScopedOverviewHideWindows& operator=(const ScopedOverviewHideWindows&) =
      delete;
  ~ScopedOverviewHideWindows() override;

  bool HasWindow(aura::Window* window) const;
  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window, bool show_window);
  void RemoveAllWindows();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

 private:
  std::map<aura::Window*, bool> window_visibility_;
  const bool force_hidden_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_HIDE_WINDOWS_H_
