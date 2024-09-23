// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_H_
#define CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/window_controller.h"

class ExtensionFunction;

namespace extensions {

class WindowControllerListObserver;

// Class to maintain a list of WindowControllers.
class WindowControllerList {
 public:
  using ControllerVector =
      std::vector<raw_ptr<WindowController, CtnExperimental>>;
  using const_iterator = ControllerVector::const_iterator;

  WindowControllerList();
  WindowControllerList(const WindowControllerList&) = delete;
  WindowControllerList& operator=(const WindowControllerList&) = delete;
  ~WindowControllerList();

  void AddExtensionWindow(WindowController* window);
  void RemoveExtensionWindow(WindowController* window);
  void NotifyWindowBoundsChanged(WindowController* window);

  void AddObserver(WindowControllerListObserver* observer);
  void RemoveObserver(WindowControllerListObserver* observer);

  const_iterator begin() const { return windows_.begin(); }
  const_iterator end() const { return windows_.end(); }

  bool empty() const { return windows_.empty(); }
  size_t size() const { return windows_.size(); }

  WindowController* get(size_t index) const { return windows_[index]; }

  // Returns a window matching the context the function was invoked in
  // using |filter|.
  WindowController* FindWindowForFunctionByIdWithFilter(
      const ExtensionFunction* function,
      int id,
      WindowController::TypeFilter filter) const;

  // Returns the focused or last added window matching the context the function
  // was invoked in.
  WindowController* CurrentWindowForFunction(ExtensionFunction* function) const;

  // Returns the focused or last added window matching the context the function
  // was invoked in using |filter|.
  WindowController* CurrentWindowForFunctionWithFilter(
      ExtensionFunction* function,
      WindowController::TypeFilter filter) const;

  static WindowControllerList* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WindowControllerList>;

  // Entries are not owned by this class and must be removed when destroyed.
  ControllerVector windows_;

  base::ObserverList<WindowControllerListObserver>::Unchecked observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_H_
