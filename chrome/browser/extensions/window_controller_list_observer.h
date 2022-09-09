// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_OBSERVER_H_
#define CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_OBSERVER_H_

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

 protected:
  virtual ~WindowControllerListObserver() {}
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_OBSERVER_H_
