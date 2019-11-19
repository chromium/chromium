// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/window_controller_list.h"

#include <algorithm>

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
  auto iter = std::find(windows_.begin(), windows_.end(), window);
  if (iter != windows_.end()) {
    windows_.erase(iter);
    for (auto& observer : observers_)
      observer.OnWindowControllerRemoved(window);
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
  for (auto iter = windows().begin(); iter != windows().end(); ++iter) {
    if ((*iter)->GetWindowId() == id) {
      if (windows_util::CanOperateOnWindow(function, *iter, filter))
        return *iter;
      return nullptr;
    }
  }
  return nullptr;
}

WindowController* WindowControllerList::CurrentWindowForFunction(
    const ExtensionFunction* function) const {
  return CurrentWindowForFunctionWithFilter(function,
                                            WindowController::kNoWindowFilter);
}

WindowController* WindowControllerList::CurrentWindowForFunctionWithFilter(
    const ExtensionFunction* function,
    WindowController::TypeFilter filter) const {
  WindowController* result = nullptr;
  // Returns either the focused window (if any), or the last window in the list.
  for (auto iter = windows().begin(); iter != windows().end(); ++iter) {
    if (windows_util::CanOperateOnWindow(function, *iter, filter)) {
      result = *iter;
      if (result->window()->IsActive())
        break;  // use focused window
    }
  }
  return result;
}

}  // namespace extensions
