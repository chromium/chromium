// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_ICON_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_ICON_LOADER_H_

#include "chrome/browser/extensions/menu_icon_loader.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "extensions/browser/extension_icon_manager.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace extensions {
class Extension;

// This is a wrapper class around |ExtensionIconManager| to be used for the
// Context Menus API.
class ExtensionMenuIconLoader : public MenuIconLoader {
 public:
  ExtensionMenuIconLoader();

  ExtensionMenuIconLoader(const ExtensionMenuIconLoader&) = delete;
  ExtensionMenuIconLoader& operator=(const ExtensionMenuIconLoader&) = delete;

  ~ExtensionMenuIconLoader() override;

  // MenuIconLoader implementation
  void LoadIcon(content::BrowserContext* context,
                const Extension* extension,
                const MenuItem::ExtensionKey& extension_key) override;
  gfx::Image GetIcon(const MenuItem::ExtensionKey& extensions_key) override;
  void RemoveIcon(const MenuItem::ExtensionKey& extensions_key) override;

 private:
  ExtensionIconManager icon_manager_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MENU_ICON_LOADER_H_
