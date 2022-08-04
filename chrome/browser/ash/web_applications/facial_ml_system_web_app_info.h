// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_FACIAL_ML_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_FACIAL_ML_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_background_task_info.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_type.h"
#include "chrome/browser/profiles/profile.h"
#include "ui/gfx/geometry/rect.h"

struct WebAppInstallInfo;

class FacialMLSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit FacialMLSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  gfx::Rect GetDefaultBounds(Browser*) const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldReuseExistingWindow() const override;
  bool ShouldShowNewWindowMenuOption() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForFacialMLApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_FACIAL_ML_SYSTEM_WEB_APP_INFO_H_
