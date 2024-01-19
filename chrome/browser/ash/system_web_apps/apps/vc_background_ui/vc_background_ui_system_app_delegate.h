// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SYSTEM_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SYSTEM_APP_DELEGATE_H_

#include <memory>

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

namespace ash::vc_background_ui {

class VcBackgroundUISystemAppDelegate : public SystemWebAppDelegate {
 public:
  explicit VcBackgroundUISystemAppDelegate(Profile* profile);

  // SystemWebAppDelegate:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  gfx::Size GetMinimumWindowSize() const override;
  gfx::Rect GetDefaultBounds(Browser* browser) const override;
  bool IsAppEnabled() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
  bool ShouldCaptureNavigations() const override;
};

}  // namespace ash::vc_background_ui

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_VC_BACKGROUND_UI_VC_BACKGROUND_UI_SYSTEM_APP_DELEGATE_H_
