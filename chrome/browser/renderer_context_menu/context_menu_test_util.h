// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_TEST_UTIL_H_
#define CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_TEST_UTIL_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"

namespace ui {
class MenuModel;
}

// Utilities for dealing with renderer context menus. Unlike
// render_view_context_menu_test_utils.cc, this file compiles on Android.

namespace context_menu_test_util {

// Searches `search_model` for an menu item with `command_id`. If it's found,
// the return value is true and the model and index where it appears in that
// model are returned in `found_model` and `found_index`. `found_model` may be a
// submenu. Otherwise returns false.
// TODO(crbug.com/484409663): Fix to not take a raw_ptr<>. It does so for
// compatibility with existing code.
bool GetMenuModelAndItemIndex(ui::MenuModel* search_model,
                              int command_id,
                              raw_ptr<ui::MenuModel>* found_model,
                              size_t* found_index);

}  // namespace context_menu_test_util

#endif  // CHROME_BROWSER_RENDERER_CONTEXT_MENU_CONTEXT_MENU_TEST_UTIL_H_
