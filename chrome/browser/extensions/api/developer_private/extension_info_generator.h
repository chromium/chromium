// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_

#include <stddef.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/developer_private.h"

namespace content {
class BrowserContext;
}

namespace gfx {
class Image;
}

namespace extensions {
class CommandService;
class ErrorConsole;
class Extension;
class ExtensionActionAPI;
class ExtensionPrefs;
class ExtensionSystem;
class ImageLoader;
class WarningService;

// Generates the developerPrivate api's specification for ExtensionInfo.
// This class is designed to only have one generation running at a time!
class ExtensionInfoGenerator {
 public:
  using ExtensionInfoList = std::vector<api::developer_private::ExtensionInfo>;

  using ExtensionInfosCallback = base::Callback<void(ExtensionInfoList)>;

  explicit ExtensionInfoGenerator(content::BrowserContext* context);
  ~ExtensionInfoGenerator();

  // Creates and asynchronously returns an ExtensionInfo for the given
  // |extension_id|, if the extension can be found.
  // If the extension cannot be found, an empty vector is passed to |callback|.
  void CreateExtensionInfo(const std::string& id,
                           const ExtensionInfosCallback& callback);

  // Creates and asynchronously returns a collection of ExtensionInfos,
  // optionally including disabled and terminated.
  void CreateExtensionsInfo(bool include_disabled,
                            bool include_terminated,
                            const ExtensionInfosCallback& callback);

 private:
  // Creates an ExtensionInfo for the given |extension| and |state|, and
  // asynchronously adds it to the |list|.
  void CreateExtensionInfoHelper(const Extension& extension,
                                 api::developer_private::ExtensionState state);

  // Callback for the asynchronous image loading.
  void OnImageLoaded(
      std::unique_ptr<api::developer_private::ExtensionInfo> info,
      const gfx::Image& image);

  // Returns the icon url for the default icon to use.
  const std::string& GetDefaultIconUrl(bool is_app, bool is_disabled);

  // Returns an icon url from the given image.
  std::string GetIconUrlFromImage(const gfx::Image& image);

  // Various systems, cached for convenience.
  content::BrowserContext* browser_context_;
  CommandService* command_service_;
  ExtensionSystem* extension_system_;
  ExtensionPrefs* extension_prefs_;
  ExtensionActionAPI* extension_action_api_;
  WarningService* warning_service_;
  ErrorConsole* error_console_;
  ImageLoader* image_loader_;

  // The number of pending image loads.
  size_t pending_image_loads_;

  // Default icons, cached and lazily initialized.
  std::string default_app_icon_url_;
  std::string default_extension_icon_url_;
  std::string default_disabled_app_icon_url_;
  std::string default_disabled_extension_icon_url_;

  // The list of extension infos that have been generated.
  ExtensionInfoList list_;

  // The callback to run once all infos have been created.
  ExtensionInfosCallback callback_;

  base::WeakPtrFactory<ExtensionInfoGenerator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExtensionInfoGenerator);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
