// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_FEEDBACK_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_FEEDBACK_SYSTEM_WEB_APP_INFO_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/experiences/system_web_apps/types/system_web_app_delegate.h"
#include "ui/gfx/geometry/rect.h"

namespace web_app {
struct WebAppInstallInfo;
class WebAppProvider;
}  // namespace web_app

class OSFeedbackAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit OSFeedbackAppDelegate(Profile* profile);
  ~OSFeedbackAppDelegate() override;

  // ash::SystemWebAppDelegate overrides:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldAllowScriptsToCloseWindows() const override;
  bool ShouldCaptureNavigations() const override;
  bool ShouldAllowFullscreen() const override;
  bool ShouldAllowMaximize() const override;
  bool ShouldAllowResize() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
  gfx::Rect GetDefaultBounds(ash::BrowserDelegate*) const override;
  ash::BrowserDelegate* LaunchAndNavigateSystemWebApp(
      Profile* profile,
      web_app::WebAppProvider* provider,
      const GURL& url,
      const apps::AppLaunchParams& params) const override;

 private:
  void OnScreenshotTaken(Profile* profile,
                         web_app::WebAppProvider* provider,
                         GURL url,
                         apps::AppLaunchParams params,
                         bool status) const;

  base::WeakPtrFactory<OSFeedbackAppDelegate> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_FEEDBACK_SYSTEM_WEB_APP_INFO_H_
