// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
#define CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_

#include <stddef.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/developer_private.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "extensions/browser/blocklist_state.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"

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

  using ExtensionInfosCallback = base::OnceCallback<void(ExtensionInfoList)>;

  explicit ExtensionInfoGenerator(content::BrowserContext* context);

  ExtensionInfoGenerator(const ExtensionInfoGenerator&) = delete;
  ExtensionInfoGenerator& operator=(const ExtensionInfoGenerator&) = delete;

  ~ExtensionInfoGenerator();

  // Creates and asynchronously returns an ExtensionInfo for the given
  // |extension_id|, if the extension can be found.
  // If the extension cannot be found, an empty vector is passed to |callback|.
  void CreateExtensionInfo(const ExtensionId& id,
                           ExtensionInfosCallback callback);

  // Creates and asynchronously returns a collection of ExtensionInfos,
  // optionally including disabled and terminated.
  void CreateExtensionsInfo(bool include_disabled,
                            bool include_terminated,
                            ExtensionInfosCallback callback);

  // Returns a list of URLPatterns where no pattern is completely contained by
  // another pattern in the list.
  static std::vector<URLPattern> GetDistinctHosts(
      const URLPatternSet& patterns);

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
  std::string GetDefaultIconUrl(const std::string& name);

  // Returns an icon url from the given image.
  std::string GetIconUrlFromImage(const gfx::Image& image);

  // Construct the needed information for the Extension Safety Check and
  // populate the relevant `extension_info` fields.
  void PopulateSafetyCheckInfo(
      const Extension& extension,
      bool updates_from_webstore,
      api::developer_private::ExtensionState state,
      BitMapBlocklistState blocklist_state,
      api::developer_private::ExtensionInfo& extension_info);

  // Various systems, cached for convenience.
  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<CommandService> command_service_;
  raw_ptr<ExtensionSystem> extension_system_;
  raw_ptr<ExtensionPrefs> extension_prefs_;
  raw_ptr<ExtensionActionAPI> extension_action_api_;
  raw_ptr<WarningService> warning_service_;
  raw_ptr<ErrorConsole> error_console_;
  raw_ptr<ImageLoader> image_loader_;

  // The number of pending image loads.
  size_t pending_image_loads_;

  // The list of extension infos that have been generated.
  ExtensionInfoList list_;

  // The callback to run once all infos have been created.
  ExtensionInfosCallback callback_;

  base::WeakPtrFactory<ExtensionInfoGenerator> weak_factory_{this};

  friend class ExtensionInfoGeneratorUnitTest;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DEVELOPER_PRIVATE_EXTENSION_INFO_GENERATOR_H_
