// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/window_controller.h"

#include <stddef.h>

#include <memory>

#include "base/types/cxx23_to_underlying.h"
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
  static_assert(base::to_underlying(api::windows::WindowType::kMaxValue) == 5,
                "Update extensions WindowController to match WindowType");
  return ((1 << base::to_underlying(api::windows::WindowType::kNormal)) |
          (1 << base::to_underlying(api::windows::WindowType::kPanel)) |
          (1 << base::to_underlying(api::windows::WindowType::kPopup)) |
          (1 << base::to_underlying(api::windows::WindowType::kApp)) |
          (1 << base::to_underlying(api::windows::WindowType::kDevtools)));
}

// static
WindowController::TypeFilter WindowController::GetFilterFromWindowTypes(
    const std::vector<api::windows::WindowType>& types) {
  WindowController::TypeFilter filter = kNoWindowFilter;
  for (auto& window_type : types)
    filter |= 1 << base::to_underlying(window_type);
  return filter;
}

// static
WindowController::TypeFilter WindowController::GetFilterFromWindowTypesValues(
    const base::Value::List* types) {
  WindowController::TypeFilter filter = WindowController::kNoWindowFilter;
  if (!types) {
    return filter;
  }
  for (const base::Value& type : *types) {
    if (!type.is_string()) {
      continue;
    }
    filter |= 1 << base::to_underlying(
                  api::windows::ParseWindowType(type.GetString()));
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
  TypeFilter type = 1 << base::to_underlying(
                        api::windows::ParseWindowType(GetWindowTypeText()));
  return (type & filter) != 0;
}

void WindowController::NotifyWindowBoundsChanged() {
  WindowControllerList::GetInstance()->NotifyWindowBoundsChanged(this);
}

}  // namespace extensions
