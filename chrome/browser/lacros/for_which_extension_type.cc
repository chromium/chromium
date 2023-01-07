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

/******** ForWhichExtensionType ********/

ForWhichExtensionType::ForWhichExtensionType(bool for_chrome_apps)
    : for_chrome_apps_(for_chrome_apps) {}

ForWhichExtensionType::ForWhichExtensionType(const ForWhichExtensionType& inst)
    : ForWhichExtensionType(inst.for_chrome_apps_) {}

ForWhichExtensionType::~ForWhichExtensionType() = default;

bool ForWhichExtensionType::Matches(
    const extensions::Extension* extension) const {
  if (for_chrome_apps_) {
    return lacros_extensions_util::IsExtensionApp(extension) &&
           !extensions::ExtensionAppRunsInOS(extension->id());
  }

  if (extension->is_extension()) {
    // QuickOffice extensions do not use file browser handler manifest key
    // to register their handlers for MS Office files; instead, they use
    // file_handler manifest key(like the way chrome apps do). We should
    // always publish quickoffice extensions since they are the default handlers
    // for MS Office files.
    if (extension_misc::IsQuickOfficeExtension(extension->id()))
      return true;

    // If an extension runs in ash, regardless of whether it may also run in
    // Lacros, do not publish it.
    if (extensions::ExtensionRunsInOS(extension->id()))
      return false;

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
