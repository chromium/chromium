// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SPLIT_VIEW_TEST_API_H_
#define ASH_PUBLIC_CPP_SPLIT_VIEW_TEST_API_H_

#include "ash/ash_export.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Provides access to the limited functions of SplitViewController for testing.
class ASH_EXPORT SplitViewTestApi {
 public:
  // See SplitViewController::SnapPosition.
  enum class SnapPosition { NONE, LEFT, RIGHT };

  SplitViewTestApi();

  SplitViewTestApi(const SplitViewTestApi&) = delete;
  SplitViewTestApi& operator=(const SplitViewTestApi&) = delete;

  ~SplitViewTestApi();

  // Snaps the window to left/right in the split view.
  void SnapWindow(aura::Window* window, SnapPosition snap_position);

  // Swaps left and right windows in the split view.
  void SwapWindows();

  // Gets the left and right window in the split view. May be null if there
  // isn't one.
  aura::Window* GetLeftWindow() const;
  aura::Window* GetRightWindow() const;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SPLIT_VIEW_TEST_API_H_
