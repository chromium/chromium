// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/web_applications/system_web_apps/system_web_app_delegate.h"

class Profile;

class ProjectorSystemWebAppDelegate : public web_app::SystemWebAppDelegate {
 public:
  explicit ProjectorSystemWebAppDelegate(Profile* profile);
  ProjectorSystemWebAppDelegate(const ProjectorSystemWebAppDelegate&) = delete;
  ProjectorSystemWebAppDelegate operator=(
      const ProjectorSystemWebAppDelegate&) = delete;
  ~ProjectorSystemWebAppDelegate() override;

  // web_app::SystemWebAppDelegate:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;
  bool ShouldCaptureNavigations() const override;
  gfx::Size GetMinimumWindowSize() const override;
  bool IsAppEnabled() const override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_PROJECTOR_SYSTEM_WEB_APP_INFO_H_
