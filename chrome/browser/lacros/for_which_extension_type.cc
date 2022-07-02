// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/for_which_extension_type.h"

#include "base/logging.h"
#include "chrome/browser/lacros/lacros_extensions_util.h"
#include "chrome/common/extensions/api/file_browser_handlers/file_browser_handler.h"
#include "extensions/common/extension.h"

/******** ForWhichExtensionType ********/

ForWhichExtensionType::ForWhichExtensionType(bool for_chrome_apps)
    : for_chrome_apps_(for_chrome_apps) {}

ForWhichExtensionType::ForWhichExtensionType(const ForWhichExtensionType& inst)
    : ForWhichExtensionType(inst.for_chrome_apps_) {}

ForWhichExtensionType::~ForWhichExtensionType() = default;

bool ForWhichExtensionType::Matches(
    const extensions::Extension* extension) const {
  if (for_chrome_apps_)
    return lacros_extensions_util::IsExtensionApp(extension);

  if (extension->is_extension()) {
    // This is for non-chrome-app extensions. We should only publish them if
    // they have file handlers.
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
