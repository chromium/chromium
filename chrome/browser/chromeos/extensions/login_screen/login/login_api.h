// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_

#include "base/macros.h"
#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace login_api

class LoginLaunchManagedGuestSessionFunction : public ExtensionFunction {
 public:
  LoginLaunchManagedGuestSessionFunction();

  DECLARE_EXTENSION_FUNCTION("login.launchManagedGuestSession",
                             LOGIN_LAUNCHMANAGEDGUESTSESSION)

 protected:
  ~LoginLaunchManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginLaunchManagedGuestSessionFunction);
};

class LoginExitCurrentSessionFunction : public ExtensionFunction {
 public:
  LoginExitCurrentSessionFunction();

  DECLARE_EXTENSION_FUNCTION("login.exitCurrentSession",
                             LOGIN_EXITCURRENTSESSION)

 protected:
  ~LoginExitCurrentSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginExitCurrentSessionFunction);
};

class LoginFetchDataForNextLoginAttemptFunction : public ExtensionFunction {
 public:
  LoginFetchDataForNextLoginAttemptFunction();

  DECLARE_EXTENSION_FUNCTION("login.fetchDataForNextLoginAttempt",
                             LOGIN_FETCHDATAFORNEXTLOGINATTEMPT)

 protected:
  ~LoginFetchDataForNextLoginAttemptFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LoginFetchDataForNextLoginAttemptFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
