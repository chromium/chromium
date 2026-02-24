// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MENU_MODEL_ANDROID_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MENU_MODEL_ANDROID_H_

#include <optional>
#include <utility>

#include "chrome/browser/extensions/extension_menu_model_android.h"

namespace content {
struct ContextMenuParams;
class RenderFrameHost;
}  // namespace content

namespace ui {
class MenuModel;
}  // namespace ui

namespace extensions {

// A test helper class for extension context menus on Android. The class is a
// friend of `ExtensionMenuModel` so it can access certain private members.
//
// The class is also an adapter for code that uses `TestRenderViewContextMenu`.
// The function signatures match, so a test can define a `PlatformContextMenu`
// that is either a `TestExtensionMenuModel` or `TestRenderViewContextMenu`
// based on platforms (Android vs. Win/Mac/Linux). This is similar to how
// `PlatformBrowserTest` exists to allow both `AndroidBrowserTest` and
// `InProcessBrowserTest` in the same test suite.
class TestExtensionMenuModel : public ExtensionMenuModel {
 public:
  TestExtensionMenuModel(content::RenderFrameHost& frame,
                         const content::ContextMenuParams& params);
  TestExtensionMenuModel(const TestExtensionMenuModel&) = delete;
  TestExtensionMenuModel& operator=(const TestExtensionMenuModel&) = delete;
  ~TestExtensionMenuModel() override = default;

  // Searches for a menu item with `command_id`. If it's found, returns the pair
  // (model, index). Otherwise returns std::nullopt.
  std::optional<std::pair<ui::MenuModel*, size_t>> GetMenuModelAndItemIndex(
      int command_id);

  // Named to match `TestRenderViewContextMenu`.
  ContextMenuMatcher& extension_items() { return matcher_; }

  // Named to match `TestRenderViewContextMenu`.
  ui::MenuModel* menu_model() { return this; }
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MENU_MODEL_ANDROID_H_
