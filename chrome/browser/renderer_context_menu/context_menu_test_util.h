// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_TEST_UTIL_H_

#include <stddef.h>

#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"

namespace ui {
class MenuModel;
}

// Utilities for dealing with renderer context menus. Unlike
// render_view_context_menu_test_utils.cc, this file compiles on Android.

namespace context_menu_test_util {

// Searches `search_model` for a menu item with `command_id`. If it's found,
// returns the pair (model, index). Otherwise returns std::nullopt.
std::optional<std::pair<ui::MenuModel*, size_t>> GetMenuModelAndItemIndex(
    ui::MenuModel* search_model,
    int command_id);

}  // namespace context_menu_test_util

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_TEST_UTIL_H_
