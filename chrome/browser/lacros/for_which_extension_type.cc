// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/for_which_extension_type.h"

#include "base/logging.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/web_file_handlers_info.h"

/******** ForWhichExtensionType ********/

ForWhichExtensionType::ForWhichExtensionType(bool for_chrome_apps)
    : for_chrome_apps_(for_chrome_apps) {}

ForWhichExtensionType::ForWhichExtensionType(const ForWhichExtensionType& inst)
    : ForWhichExtensionType(inst.for_chrome_apps_) {}

ForWhichExtensionType::~ForWhichExtensionType() = default;

bool ForWhichExtensionType::Matches(
    const extensions::Extension* extension) const {
  if (for_chrome_apps_) {
    // For older ash version without app service block list support, follow
    // the old code path. The extension app running in both ash and lacros will
    // not be published in app service from either ash or lacros.
    if (!extensions::IsAppServiceBlocklistCrosapiSupported()) {
      return lacros_extensions_util::IsExtensionApp(extension) &&
             !extensions::ExtensionAppRunsInOS(extension->id());
    }

    // An extension app should be published to app service by lacros if it
    // meets all of the following conditions:
    // 1. It does not run in ash only.
    // 2. If it runs in both ash and lacros, it is not blocked for app service
    // in lacros.
    return lacros_extensions_util::IsExtensionApp(extension) &&
           !extensions::ExtensionAppRunsInOSOnly(extension->id()) &&
           !(extensions::ExtensionAppRunsInBothOSAndStandaloneBrowser(
                 extension->id()) &&
             extensions::
                 ExtensionAppBlockListedForAppServiceInStandaloneBrowser(
                     extension->id()));
  }

  if (extension->is_extension()) {
    // Web File Handlers use the `file_handlers` manifest key for registration.
    if (extensions::WebFileHandlers::SupportsWebFileHandlers(*extension)) {
      return true;
    }

    // QuickOffice extensions do not use file browser handler manifest key
    // to register their handlers for MS Office files; instead, they use
    // file_handler manifest key (like the way chrome apps do). We should
    // always publish quickoffice extensions since they are the default handlers
    // for MS Office files.
    if (extension_misc::IsQuickOfficeExtension(extension->id())) {
      return true;
    }

    if (!extensions::IsAppServiceBlocklistCrosapiSupported()) {
      // For older ash version without app service block list support, follow
      // the old code path. It won't publish the extension if it runs in ash,
      // even if it may also runs in lacros.
      if (extensions::ExtensionRunsInOS(extension->id())) {
        return false;
      }
    } else {
      // An extension should be published to app service by lacros if it meets
      // all of the following conditions:
      // 1. It does not run in ash only.
      // 2. If it runs in both ash and lacros, it is not blocked for app service
      // in lacros.

      if (extensions::ExtensionRunsInOSOnly(extension->id())) {
        return false;
      }

      if (extensions::ExtensionRunsInBothOSAndStandaloneBrowser(
              extension->id()) &&
          extensions::ExtensionBlockListedForAppServiceInStandaloneBrowser(
              extension->id())) {
        return false;
      }
    }

    // For the regular extensions, we should only publish them if they have file
    // handlers registered using file browser handlers.
    FileBrowserHandler::List* handler_list =
        FileBrowserHandler::GetHandlers(extension);
    return handler_list;
  }

  return false;
}

/******** Utilities ********/

ForWhichExtensionType InitForChromeApps() {
  return ForWhichExtensionType(/* for_chrome_apps_*/ true);
}

ForWhichExtensionType InitForExtensions() {
  return ForWhichExtensionType(/* for_chrome_apps_*/ false);
}
