// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ICON_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ICON_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

class ExtensionIconManager {
 public:
  class Observer {
   public:
    virtual void OnImageLoaded(const std::string& extension_id) = 0;
  };

  ExtensionIconManager();
  virtual ~ExtensionIconManager();

  // Start loading the icon for the given extension.
  void LoadIcon(content::BrowserContext* context,
                const extensions::Extension* extension);

  // This returns an image of width/height kFaviconSize, loaded either from an
  // entry specified in the extension's 'icon' section of the manifest, or a
  // default extension icon.
  gfx::Image GetIcon(const std::string& extension_id);

  // Removes the extension's icon from memory.
  void RemoveIcon(const std::string& extension_id);

  void set_monochrome(bool value) { monochrome_ = value; }
  void set_observer(Observer* observer) { observer_ = observer; }

 private:
  void OnImageLoaded(const std::string& extension_id, const gfx::Image& image);

  // Makes sure we've done one-time initialization of the default extension icon
  // default_icon_.
  void EnsureDefaultIcon();

  // Maps extension id to the icon for that extension.
  std::map<std::string, gfx::Image> icons_;

  // Set of extension IDs waiting for icons to load.
  std::set<std::string> pending_icons_;

  // The default icon we'll use if an extension doesn't have one.
  gfx::Image default_icon_;

  // If true, we will desaturate the icons to make them monochromatic.
  bool monochrome_ = false;

  Observer* observer_ = nullptr;

  base::WeakPtrFactory<ExtensionIconManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionIconManager);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ICON_MANAGER_H_
