// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MENU_ICON_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MENU_ICON_LOADER_H_

#include "chrome/browser/extensions/extension_menu_icon_loader.h"

namespace extensions {

class TestExtensionMenuIconLoader : public ExtensionMenuIconLoader {
 public:
  TestExtensionMenuIconLoader() = default;
  ~TestExtensionMenuIconLoader() override = default;

  TestExtensionMenuIconLoader(const TestExtensionMenuIconLoader&) = delete;
  TestExtensionMenuIconLoader& operator=(const TestExtensionMenuIconLoader&) =
      delete;

  // ExtensionMenuIconLoader implementation
  void LoadIcon(content::BrowserContext* context,
                const Extension* extension,
                const MenuItem::ExtensionKey& extension_key) override;
  gfx::Image GetIcon(const MenuItem::ExtensionKey& extension_key) override;
  void RemoveIcon(const MenuItem::ExtensionKey& extension_key) override;

  void Reset();

  int load_icon_calls() { return load_icon_calls_; }
  int get_icon_calls() { return get_icon_calls_; }
  int remove_icon_calls() { return remove_icon_calls_; }

 private:
  int load_icon_calls_ = 0;
  int get_icon_calls_ = 0;
  int remove_icon_calls_ = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_MENU_ICON_LOADER_H_
