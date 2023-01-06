// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_FIRST_RUN_HELPER_H_
#define ASH_PUBLIC_CPP_FIRST_RUN_HELPER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"

namespace gfx {
class Rect;
}

namespace ash {

// Allows clients to control pieces of the UI used in first-run tutorials.
// Methods exist here instead of on the Shelf or SystemTray interfaces due to
// small behavior differences (all methods only affect the primary display,
// opening the system tray bubble is persistent, etc.).
class ASH_EXPORT FirstRunHelper {
 public:
  virtual ~FirstRunHelper() = default;

  // Cleans up the ash UI on tutorial start. Returns the instance for further
  // method calls. Destroying the instance restores the ash UI on tutorial.
  // |on_cancelled| will be called when something happened inside ash that
  // should cancel the tutorial (e.g. the device is shutting down).
  static std::unique_ptr<FirstRunHelper> Start(base::OnceClosure on_cancelled);

  // Returns the bounds of the home button on the primary display in screen
  // coordinates. Returns empty bounds if the home button is not shown in shelf.
  virtual gfx::Rect GetAppListButtonBounds() = 0;

  // Opens the system tray bubble menu to show the default view. Does nothing if
  // the bubble is already open. The bubble stays open until explicitly closed.
  // Returns bubble bounds in screen coordinates.
  virtual gfx::Rect OpenTrayBubble() = 0;

  // Closes the system tray bubble menu. Does nothing if the bubble is already
  // closed.
  virtual void CloseTrayBubble() = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_FIRST_RUN_HELPER_H_
