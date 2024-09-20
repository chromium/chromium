// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_GRADUATION_APP_DELEGATE_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_GRADUATION_APP_DELEGATE_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

namespace web_app {
struct WebAppInstallInfo;
}

namespace ash::graduation {

class GraduationAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit GraduationAppDelegate(Profile* profile);

  // ash::SystemWebAppDelegate:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldShowInLauncher() const override;
  bool IsAppEnabled() const override;
  bool ShouldCaptureNavigations() const override;
};

}  // namespace ash::graduation
#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_GRADUATION_APP_DELEGATE_H_
