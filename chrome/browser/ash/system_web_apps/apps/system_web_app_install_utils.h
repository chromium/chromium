// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_SYSTEM_WEB_APP_INSTALL_UTILS_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_SYSTEM_WEB_APP_INSTALL_UTILS_H_

#include <initializer_list>
#include <memory>
#include <string>

#include "chrome/browser/web_applications/web_app_install_info.h"

class GURL;

namespace web_app {

struct IconResourceInfo {
  std::string icon_name;
  SquareSizePx size;
  int resource_id;
};

// Create the icon info struct for a system web app from a resource id. We don't
// actually download the icon, so the app_url and icon_name are just used as a
// key.
void CreateIconInfoForSystemWebApp(
    const GURL& app_url,
    const std::initializer_list<IconResourceInfo>& manifest_icons,
    WebAppInstallInfo& web_app_info);

// Create shortcuts menu item for a system web app from a resource id. The icons
// aren't downloaded, `icon_name` in IconResourceInfo is used as a key.
void CreateShortcutsMenuItemForSystemWebApp(
    const std::u16string& name,
    const GURL& shortcut_url,
    const std::initializer_list<IconResourceInfo>& shortcut_menu_item_icons,
    WebAppInstallInfo& web_app_info);

// Get correct ChromeOS background color based on if dark mode is requested and
// if kSemanticColorsDebugOverride is enabled.
SkColor GetDefaultBackgroundColor(const bool use_dark_mode);

// Creates a `WebAppInstallInfo` for a System Web App where the `manifest_id` is
// derived from the `start_url`. Changing the `start_url` will cause the
// resulting SWA's identity to change, so if a change is neededthe regular
// `WebAppInstallInfo` constructor must be used with the original `manifest_id`.
std::unique_ptr<WebAppInstallInfo>
CreateSystemWebAppInstallInfoWithStartUrlAsIdentity(const GURL& start_url);

}  // namespace web_app

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_SYSTEM_WEB_APP_INSTALL_UTILS_H_
