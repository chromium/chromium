// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SPLIT_VIEW_TEST_API_H_
#define ASH_PUBLIC_CPP_SPLIT_VIEW_TEST_API_H_

#include "ash/ash_export.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_types.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Provides access to the limited functions of SplitViewController for testing.
class ASH_EXPORT SplitViewTestApi {
 public:
  SplitViewTestApi();

  SplitViewTestApi(const SplitViewTestApi&) = delete;
  SplitViewTestApi& operator=(const SplitViewTestApi&) = delete;

  ~SplitViewTestApi();

  // Snaps the window to the `snap_position` in the split view.
  void SnapWindow(aura::Window* window, SnapPosition snap_position);

  // Swaps primary and secondary windows in the split view.
  void SwapWindows();

  // Gets the primary and secondary window in the split view. Returns null if
  // there isn't one.
  aura::Window* GetPrimaryWindow() const;
  aura::Window* GetSecondaryWindow() const;

  const raw_ptr<SplitViewController> controller_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SPLIT_VIEW_TEST_API_H_
