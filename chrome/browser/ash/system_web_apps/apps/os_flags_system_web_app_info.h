// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_FLAGS_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_FLAGS_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

class Profile;

// This is the web app Flags which is called from Lacros and serves Ash's
// chrome:// URLs as web applications.
// To allow users to call Ash's pages directly, they can use os://<url> which
// will then be handled by this app.
class OsFlagsSystemWebAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit OsFlagsSystemWebAppDelegate(Profile* profile);
  OsFlagsSystemWebAppDelegate(const OsFlagsSystemWebAppDelegate&) = delete;
  OsFlagsSystemWebAppDelegate operator=(const OsFlagsSystemWebAppDelegate&) =
      delete;
  ~OsFlagsSystemWebAppDelegate() override;

  // ash::SystemWebAppDelegate:
  std::unique_ptr<web_app::WebAppInstallInfo> GetWebAppInfo() const override;

  // TODO(crbug.com/40201849) - Add override for GetAdditionalSearchTerms() to
  // allow capturing the os:// search tearms to be used.
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearchAndShelf() const override;
};

#endif  // CHROME_BROWSER_ASH_SYSTEM_WEB_APPS_APPS_OS_FLAGS_SYSTEM_WEB_APP_INFO_H_
