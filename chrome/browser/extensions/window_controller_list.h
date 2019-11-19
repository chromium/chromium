// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_H_
#define CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_H_

#include <list>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/window_controller.h"

class ExtensionFunction;

namespace extensions {

class WindowControllerListObserver;

// Class to maintain a list of WindowControllers.
class WindowControllerList {
 public:
  typedef std::list<WindowController*> ControllerList;

  WindowControllerList();
  ~WindowControllerList();

  void AddExtensionWindow(WindowController* window);
  void RemoveExtensionWindow(WindowController* window);

  void AddObserver(WindowControllerListObserver* observer);
  void RemoveObserver(WindowControllerListObserver* observer);

  // Returns a window matching the context the function was invoked in
  // using |filter|.
  WindowController* FindWindowForFunctionByIdWithFilter(
      const ExtensionFunction* function,
      int id,
      WindowController::TypeFilter filter) const;

  // Returns the focused or last added window matching the context the function
  // was invoked in.
  WindowController* CurrentWindowForFunction(
      const ExtensionFunction* function) const;

  // Returns the focused or last added window matching the context the function
  // was invoked in using |filter|.
  WindowController* CurrentWindowForFunctionWithFilter(
      const ExtensionFunction* function,
      WindowController::TypeFilter filter) const;

  const ControllerList& windows() const { return windows_; }

  static WindowControllerList* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<WindowControllerList>;

  // Entries are not owned by this class and must be removed when destroyed.
  ControllerList windows_;

  base::ObserverList<WindowControllerListObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(WindowControllerList);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WINDOW_CONTROLLER_LIST_H_
