// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_RECORDER_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_RECORDER_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"
#include "ui/gfx/geometry/rect.h"

class Browser;

namespace web_app {
struct WebAppInstallInfo;
}  // namespace web_app

class RecorderSystemAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit RecorderSystemAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate overrides
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool IsAppEnabled() const override;
};

// Return a WebAppInstallInfo used to install the app.
std::unique_ptr<web_app::WebAppInstallInfo>
CreateWebAppInfoForRecorderSystemWebApp();

// Returns the default bounds.
gfx::Rect GetDefaultBoundsForRecorderApp(Browser*);

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_RECORDER_APP_RECORDER_SYSTEM_WEB_APP_INFO_H_
