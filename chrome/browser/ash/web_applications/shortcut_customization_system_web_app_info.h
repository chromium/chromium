// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_SHORTCUT_CUSTOMIZATION_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_SHORTCUT_CUSTOMIZATION_SYSTEM_WEB_APP_INFO_H_

#include "ash/webui/shortcut_customization_ui/url_constants.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "ui/gfx/geometry/size.h"

struct WebAppInstallInfo;

class ShortcutCustomizationSystemAppDelegate
    : public ash::SystemWebAppDelegate {
 public:
  explicit ShortcutCustomizationSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool IsAppEnabled() const override;
};
// Returns a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo>
CreateWebAppInfoForShortcutCustomizationSystemWebApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_SHORTCUT_CUSTOMIZATION_SYSTEM_WEB_APP_INFO_H_
