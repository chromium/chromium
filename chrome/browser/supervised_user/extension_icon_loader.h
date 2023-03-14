// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXTENSION_ICON_LOADER_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXTENSION_ICON_LOADER_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"

namespace content {
class BrowserContext;
}

namespace gfx {
class Image;
class ImageSkia;
}  // namespace gfx

namespace extensions {
class Extension;

// Loads an extension icon from local resources. Only installed extensions will
// have a local resource available. If the local resource is not available, a
// default icon is returned.
class ExtensionIconLoader {
 public:
  ExtensionIconLoader();
  ExtensionIconLoader(const ExtensionIconLoader&) = delete;
  ExtensionIconLoader& operator=(const ExtensionIconLoader&) = delete;
  ~ExtensionIconLoader();

  using IconLoadCallback = base::OnceCallback<void(const gfx::ImageSkia& icon)>;

  // Attempts to load an extension icon from local resources.
  void Load(const extensions::Extension& extension,
            content::BrowserContext* context,
            IconLoadCallback icon_load_callback);

 private:
  void OnIconLoaded(bool is_app, const gfx::Image& image);

  IconLoadCallback icon_load_callback_;
  base::WeakPtrFactory<ExtensionIconLoader> factory_{this};
};
}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXTENSION_ICON_LOADER_H_
