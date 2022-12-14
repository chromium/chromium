// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_EXTENSION_APP_UTILS_H_
#define CHROME_BROWSER_ASH_APP_LIST_EXTENSION_APP_UTILS_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

namespace ui {
class SimpleMenuModel;
}

namespace app_list {

bool ShouldShowInLauncher(const extensions::Extension* extension,
                          content::BrowserContext* context);

// chrome.contextMenus API does not support menu item icons. This function
// compensates for that by adding icons to menus for prominent system apps
// (currently just Files app)
void AddMenuItemIconsForSystemApps(const std::string& app_id,
                                   ui::SimpleMenuModel* menu_model,
                                   int start_index,
                                   int count);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_EXTENSION_APP_UTILS_H_
