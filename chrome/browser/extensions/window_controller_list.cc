// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/window_controller_list.h"

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/extensions/api/tabs/windows_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/extensions/window_controller_list_observer.h"
#include "chrome/common/extensions/api/windows.h"
#include "components/sessions/core/session_id.h"
#include "extensions/browser/extension_function.h"
#include "ui/base/base_window.h"

namespace extensions {

///////////////////////////////////////////////////////////////////////////////
// WindowControllerList

// static
WindowControllerList* WindowControllerList::GetInstance() {
  return base::Singleton<WindowControllerList>::get();
}

WindowControllerList::WindowControllerList() {
}

WindowControllerList::~WindowControllerList() {
}

void WindowControllerList::AddExtensionWindow(WindowController* window) {
  windows_.push_back(window);
  for (auto& observer : observers_)
    observer.OnWindowControllerAdded(window);
}

void WindowControllerList::RemoveExtensionWindow(WindowController* window) {
  auto iter = base::ranges::find(windows_, window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
    for (auto& observer : observers_)
      observer.OnWindowControllerRemoved(window);
  }
}

void WindowControllerList::NotifyWindowBoundsChanged(WindowController* window) {
  if (base::Contains(windows_, window)) {
    for (auto& observer : observers_)
      observer.OnWindowBoundsChanged(window);
  }
}

void WindowControllerList::AddObserver(WindowControllerListObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowControllerList::RemoveObserver(
    WindowControllerListObserver* observer) {
  observers_.RemoveObserver(observer);
}

WindowController* WindowControllerList::FindWindowForFunctionByIdWithFilter(
    const ExtensionFunction* function,
    int id,
    WindowController::TypeFilter filter) const {
  for (auto iter = windows_.begin(); iter != windows_.end(); ++iter) {
    if ((*iter)->GetWindowId() == id) {
      if (windows_util::CanOperateOnWindow(function, *iter, filter))
        return *iter;
      return nullptr;
    }
  }
  return nullptr;
}

WindowController* WindowControllerList::CurrentWindowForFunction(
    ExtensionFunction* function) const {
  return CurrentWindowForFunctionWithFilter(function,
                                            WindowController::kNoWindowFilter);
}

WindowController* WindowControllerList::CurrentWindowForFunctionWithFilter(
    ExtensionFunction* function,
    WindowController::TypeFilter filter) const {
  // Always prefer the focused window if available. If there is no focused
  // window, prefer the window to which the sender window is logically parented.
  // Since the browser window is not "focused" when an extension popup is open
  // (because popup is hosted in a separate window, which is focused instead),
  // we need to check for the logical parent window here. If neither of these
  // are available, return the last window.
  WindowController* last_window = nullptr;
  WindowController* parent_window = nullptr;

  for (const auto& controller : windows_) {
    if (!windows_util::CanOperateOnWindow(function, controller, filter)) {
      continue;
    }

    if (controller->window()->IsActive()) {
      // If the window is focused, return it immediately.
      return controller;
    }

    if (windows_util::CalledFromChildWindow(function, controller)) {
      parent_window = controller;
    }

    last_window = controller;
  }

  return parent_window ? parent_window : last_window;
}

}  // namespace extensions
