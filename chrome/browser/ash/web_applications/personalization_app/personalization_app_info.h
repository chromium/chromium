// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_INFO_H_

#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
struct WebApplicationInfo;

class PersonalizationSystemAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit PersonalizationSystemAppDelegate(Profile* profile);

  // web_app::SystemWebAppDelegate overrides:
  std::unique_ptr<WebApplicationInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
  bool ShouldShowInLauncher() const override;
};

// Return a WebApplicationInfo used to install the app.
std::unique_ptr<WebApplicationInfo> CreateWebAppInfoForPersonalizationApp();

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PERSONALIZATION_APP_PERSONALIZATION_APP_INFO_H_
