// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CAMERA_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CAMERA_SYSTEM_WEB_APP_INFO_H_

#include <memory>
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"

#include "ui/gfx/geometry/rect.h"

class Browser;
struct WebAppInstallInfo;

class CameraSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit CameraSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  gfx::Rect GetDefaultBounds(Browser* browser) const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<WebAppInstallInfo> CreateWebAppInfoForCameraSystemWebApp();

// Returns the default bounds.
gfx::Rect GetDefaultBoundsForCameraApp(Browser*);

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CAMERA_APP_CAMERA_SYSTEM_WEB_APP_INFO_H_
