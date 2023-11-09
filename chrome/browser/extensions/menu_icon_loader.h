// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_MENU_ICON_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_MENU_ICON_LOADER_H_

#include "chrome/browser/extensions/menu_manager.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace extensions {
class Extension;

// This is an interface class to be used in the Context Menus API code to load
// the icon to use for the context menu.
class MenuIconLoader {
 public:
  virtual ~MenuIconLoader() = default;
  // Starts loading the icon for the context described by |context|,
  // |extension|, and |extension_key|.
  virtual void LoadIcon(content::BrowserContext* context,
                        const Extension* extension,
                        const MenuItem::ExtensionKey& extension_key) = 0;

  // Returns an image of width/height kFaviconSize.
  virtual gfx::Image GetIcon(const MenuItem::ExtensionKey& extension_key) = 0;

  // Removes the previously loaded icon from memory.
  virtual void RemoveIcon(const MenuItem::ExtensionKey& extension_key) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_MENU_ICON_LOADER_H_
