// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_OBSERVER_H_

#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class WindowController;

// Implementations must not change the contents of the WindowControllerList
// inside any of these methods.
class WindowControllerListObserver {
  public:
  // Called immediately after a window controller is added to the list
  virtual void OnWindowControllerAdded(WindowController* window_controller) {}

  // Called immediately after a window controller is removed from the list
  virtual void OnWindowControllerRemoved(WindowController* window_controller) {}

  // Called when new bounds are committed.
  virtual void OnWindowBoundsChanged(WindowController* window_controller) {}

  // Called when a window's focus is changed.
  // As of Sep 23, 2025, this API was only used on desktop Android.
  //
  // TODO(http://crbug.com/446925633): Use this API on non-Android OSes.
  virtual void OnWindowFocusChanged(WindowController* window_controller,
                                    bool has_focus) {}

 protected:
  virtual ~WindowControllerListObserver() = default;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_OBSERVER_H_
