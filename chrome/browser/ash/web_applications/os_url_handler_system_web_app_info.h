// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_URL_HANDLER_SYSTEM_WEB_APP_INFO_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_URL_HANDLER_SYSTEM_WEB_APP_INFO_H_

#include "chrome/browser/ash/system_web_apps/types/system_web_app_delegate.h"

class Profile;

// This is the web app handler which is called from Lacros and serves Ash's
// chrome:// URLs as web applications.
// To allow users to call Ash's pages directly, they can use os://<url> which
// will then be handled by this app.
class OsUrlHandlerSystemWebAppDelegate : public ash::SystemWebAppDelegate {
 public:
  explicit OsUrlHandlerSystemWebAppDelegate(Profile* profile);
  OsUrlHandlerSystemWebAppDelegate(const OsUrlHandlerSystemWebAppDelegate&) =
      delete;
  OsUrlHandlerSystemWebAppDelegate operator=(
      const OsUrlHandlerSystemWebAppDelegate&) = delete;
  ~OsUrlHandlerSystemWebAppDelegate() override;

  // ash::SystemWebAppDelegate:
  std::unique_ptr<WebAppInstallInfo> GetWebAppInfo() const override;

  // TODO(crbug/1260386) - Add override for GetAdditionalSearchTerms() to allow
  // capturing the os:// search tearms to be used.
  bool ShouldCaptureNavigations() const override;
  bool IsAppEnabled() const override;
  bool ShouldShowInLauncher() const override;
  bool ShouldShowInSearch() const override;
  Browser* GetWindowForLaunch(Profile* profile, const GURL& url) const override;
  bool ShouldRestoreOverrideUrl() const override;
  bool IsUrlInSystemAppScope(const GURL& url) const override;

  // Can be called by a test to enforce the app to be enabled.
  static void EnableDelegateForTesting(bool enable);
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_OS_URL_HANDLER_SYSTEM_WEB_APP_INFO_H_
