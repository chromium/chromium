// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/window_controller.h"

#include <stddef.h>

#include <memory>

#include "base/values.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/windows.h"

namespace extensions {

///////////////////////////////////////////////////////////////////////////////
// WindowController

// static
WindowController::TypeFilter WindowController::GetAllWindowFilter() {
  // This needs to be updated if there is a change to
  // extensions::api::windows:WindowType.
  static_assert(api::windows::WINDOW_TYPE_LAST == 5,
                "Update extensions WindowController to match WindowType");
  return ((1 << api::windows::WINDOW_TYPE_NORMAL) |
          (1 << api::windows::WINDOW_TYPE_PANEL) |
          (1 << api::windows::WINDOW_TYPE_POPUP) |
          (1 << api::windows::WINDOW_TYPE_APP) |
          (1 << api::windows::WINDOW_TYPE_DEVTOOLS));
}

// static
WindowController::TypeFilter WindowController::GetFilterFromWindowTypes(
    const std::vector<api::windows::WindowType>& types) {
  WindowController::TypeFilter filter = kNoWindowFilter;
  for (auto& window_type : types)
    filter |= 1 << window_type;
  return filter;
}

// static
WindowController::TypeFilter WindowController::GetFilterFromWindowTypesValues(
    const base::Value::List* types) {
  WindowController::TypeFilter filter = WindowController::kNoWindowFilter;
  if (!types)
    return filter;
  for (const base::Value& type : *types) {
    if (!type.is_string())
      continue;
    filter |= 1 << api::windows::ParseWindowType(type.GetString());
  }
  return filter;
}

WindowController::WindowController(ui::BaseWindow* window, Profile* profile)
    : window_(window), profile_(profile) {
}

WindowController::~WindowController() {
}

Browser* WindowController::GetBrowser() const {
  return nullptr;
}

bool WindowController::MatchesFilter(TypeFilter filter) const {
  TypeFilter type = 1 << api::windows::ParseWindowType(GetWindowTypeText());
  return (type & filter) != 0;
}

void WindowController::NotifyWindowBoundsChanged() {
  WindowControllerList::GetInstance()->NotifyWindowBoundsChanged(this);
}

}  // namespace extensions
