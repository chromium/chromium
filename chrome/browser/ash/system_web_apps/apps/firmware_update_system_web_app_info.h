// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FIRMWARE_UPDATE_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FIRMWARE_UPDATE_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class FirmwareUpdateSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit FirmwareUpdateSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldAllowFullscreen() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldAllowResize() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
  gfx::Rect GetDefaultBounds(ash::BrowserDelegate*) const override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_FIRMWARE_UPDATE_SYSTEM_WEB_APP_INFO_H_
