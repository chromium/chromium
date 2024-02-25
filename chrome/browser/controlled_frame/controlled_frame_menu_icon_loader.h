// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MENU_ICON_LOADER_H_
#define CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MENU_ICON_LOADER_H_

#include <map>
#include <set>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/menu_icon_loader.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/web_applications/web_app_install_info.h"

class SkBitmap;

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace extensions {
class Extension;
}  // namespace extensions

namespace controlled_frame {

// This class implements the extensions::MenuIconLoader interface to allow a
// Controlled Frame instance embedded by an Isolated Web App (IWA) to provide
// the IWA's icon to use for the Context Menus API.
class ControlledFrameMenuIconLoader : public extensions::MenuIconLoader {
 public:
  ControlledFrameMenuIconLoader();

  ControlledFrameMenuIconLoader(const ControlledFrameMenuIconLoader&) = delete;
  ControlledFrameMenuIconLoader& operator=(
      const ControlledFrameMenuIconLoader&) = delete;

  ~ControlledFrameMenuIconLoader() override;

  // MenuIconLoader implementation
  void LoadIcon(
      content::BrowserContext* context,
      const extensions::Extension* extension,
      const extensions::MenuItem::ExtensionKey& extension_key) override;
  gfx::Image GetIcon(
      const extensions::MenuItem::ExtensionKey& extensions_key) override;
  void RemoveIcon(
      const extensions::MenuItem::ExtensionKey& extensions_key) override;

  void SetNotifyOnLoadedCallbackForTesting(base::RepeatingClosure callback);

 private:
  friend class ControlledFrameMenuIconLoader;
  FRIEND_TEST_ALL_PREFIXES(ControlledFrameMenuIconLoaderTest,
                           LoadGetAndRemoveIcon);
  FRIEND_TEST_ALL_PREFIXES(ControlledFrameMenuIconLoaderTest, MenuManager);

  void OnIconLoaded(const extensions::MenuItem::ExtensionKey& extension_key,
                    web_app::IconPurpose purpose,
                    SkBitmap bitmap);

  std::map<extensions::MenuItem::ExtensionKey, gfx::Image> icons_;
  std::set<extensions::MenuItem::ExtensionKey> pending_icons_;

  base::RepeatingClosure on_loaded_callback_;

  base::WeakPtrFactory<ControlledFrameMenuIconLoader> weak_ptr_factory_{this};
};

}  // namespace controlled_frame

#endif  // CHROME_BROWSER_CONTROLLED_FRAME_CONTROLLED_FRAME_MENU_ICON_LOADER_H_
