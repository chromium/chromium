// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_

#include <optional>

#include "chromeos/crosapi/mojom/login.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class ExtensionFunctionWithOptionalErrorResult : public ExtensionFunction {
 protected:
  ~ExtensionFunctionWithOptionalErrorResult() override;

  void OnResult(const std::optional<std::string>& error);
};

class ExtensionFunctionWithStringResult : public ExtensionFunction {
 protected:
  ~ExtensionFunctionWithStringResult() override;

  void OnResult(const std::string& result);
};

class ExtensionFunctionWithVoidResult : public ExtensionFunction {
 protected:
  ~ExtensionFunctionWithVoidResult() override;

  void OnResult();
};

class LoginLaunchManagedGuestSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginLaunchManagedGuestSessionFunction();

  LoginLaunchManagedGuestSessionFunction(
      const LoginLaunchManagedGuestSessionFunction&) = delete;

  LoginLaunchManagedGuestSessionFunction& operator=(
      const LoginLaunchManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.launchManagedGuestSession",
                             LOGIN_LAUNCHMANAGEDGUESTSESSION)

 protected:
  ~LoginLaunchManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginExitCurrentSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginExitCurrentSessionFunction();

  LoginExitCurrentSessionFunction(const LoginExitCurrentSessionFunction&) =
      delete;

  LoginExitCurrentSessionFunction& operator=(
      const LoginExitCurrentSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.exitCurrentSession",
                             LOGIN_EXITCURRENTSESSION)

 protected:
  ~LoginExitCurrentSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginFetchDataForNextLoginAttemptFunction
    : public ExtensionFunctionWithStringResult {
 public:
  LoginFetchDataForNextLoginAttemptFunction();

  LoginFetchDataForNextLoginAttemptFunction(
      const LoginFetchDataForNextLoginAttemptFunction&) = delete;

  LoginFetchDataForNextLoginAttemptFunction& operator=(
      const LoginFetchDataForNextLoginAttemptFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.fetchDataForNextLoginAttempt",
                             LOGIN_FETCHDATAFORNEXTLOGINATTEMPT)

 protected:
  ~LoginFetchDataForNextLoginAttemptFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginLockManagedGuestSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginLockManagedGuestSessionFunction();

  LoginLockManagedGuestSessionFunction(
      const LoginLockManagedGuestSessionFunction&) = delete;

  LoginLockManagedGuestSessionFunction& operator=(
      const LoginLockManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.lockManagedGuestSession",
                             LOGIN_LOCKMANAGEDGUESTSESSION)

 protected:
  ~LoginLockManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginUnlockManagedGuestSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginUnlockManagedGuestSessionFunction();

  LoginUnlockManagedGuestSessionFunction(
      const LoginUnlockManagedGuestSessionFunction&) = delete;

  LoginUnlockManagedGuestSessionFunction& operator=(
      const LoginUnlockManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.unlockManagedGuestSession",
                             LOGIN_UNLOCKMANAGEDGUESTSESSION)

 protected:
  ~LoginUnlockManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginLockCurrentSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginLockCurrentSessionFunction();

  LoginLockCurrentSessionFunction(const LoginLockCurrentSessionFunction&) =
      delete;

  LoginLockCurrentSessionFunction& operator=(
      const LoginLockCurrentSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.lockCurrentSession",
                             LOGIN_LOCKCURRENTSESSION)

 protected:
  ~LoginLockCurrentSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginUnlockCurrentSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginUnlockCurrentSessionFunction();

  LoginUnlockCurrentSessionFunction(const LoginUnlockCurrentSessionFunction&) =
      delete;

  LoginUnlockCurrentSessionFunction& operator=(
      const LoginUnlockCurrentSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.unlockCurrentSession",
                             LOGIN_UNLOCKCURRENTSESSION)

 protected:
  ~LoginUnlockCurrentSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginLaunchSamlUserSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginLaunchSamlUserSessionFunction();

  LoginLaunchSamlUserSessionFunction(
      const LoginLaunchSamlUserSessionFunction&) = delete;

  LoginLaunchSamlUserSessionFunction& operator=(
      const LoginLaunchSamlUserSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.launchSamlUserSession",
                             LOGIN_LAUNCHSAMLUSERSESSION)

 protected:
  ~LoginLaunchSamlUserSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginLaunchSharedManagedGuestSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginLaunchSharedManagedGuestSessionFunction();

  LoginLaunchSharedManagedGuestSessionFunction(
      const LoginLaunchSharedManagedGuestSessionFunction&) = delete;

  LoginLaunchSharedManagedGuestSessionFunction& operator=(
      const LoginLaunchSharedManagedGuestSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.launchSharedManagedGuestSession",
                             LOGIN_LAUNCHSHAREDMANAGEDGUESTSESSION)

 protected:
  ~LoginLaunchSharedManagedGuestSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginEnterSharedSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginEnterSharedSessionFunction();

  LoginEnterSharedSessionFunction(const LoginEnterSharedSessionFunction&) =
      delete;

  LoginEnterSharedSessionFunction& operator=(
      const LoginEnterSharedSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.enterSharedSession",
                             LOGIN_ENTERSHAREDSESSION)

 protected:
  ~LoginEnterSharedSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginUnlockSharedSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginUnlockSharedSessionFunction();

  LoginUnlockSharedSessionFunction(const LoginUnlockSharedSessionFunction&) =
      delete;

  LoginUnlockSharedSessionFunction& operator=(
      const LoginUnlockSharedSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.unlockSharedSession",
                             LOGIN_UNLOCKSHAREDSESSION)

 protected:
  ~LoginUnlockSharedSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginEndSharedSessionFunction
    : public ExtensionFunctionWithOptionalErrorResult {
 public:
  LoginEndSharedSessionFunction();

  LoginEndSharedSessionFunction(const LoginEndSharedSessionFunction&) = delete;

  LoginEndSharedSessionFunction& operator=(
      const LoginEndSharedSessionFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.endSharedSession", LOGIN_ENDSHAREDSESSION)

 protected:
  ~LoginEndSharedSessionFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginSetDataForNextLoginAttemptFunction
    : public ExtensionFunctionWithVoidResult {
 public:
  LoginSetDataForNextLoginAttemptFunction();

  LoginSetDataForNextLoginAttemptFunction(
      const LoginSetDataForNextLoginAttemptFunction&) = delete;

  LoginSetDataForNextLoginAttemptFunction& operator=(
      const LoginSetDataForNextLoginAttemptFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.setDataForNextLoginAttempt",
                             LOGIN_SETDATAFORNEXTLOGINATTEMPT)

 protected:
  ~LoginSetDataForNextLoginAttemptFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginRequestExternalLogoutFunction : public ExtensionFunction {
 public:
  LoginRequestExternalLogoutFunction();

  LoginRequestExternalLogoutFunction(
      const LoginRequestExternalLogoutFunction&) = delete;

  LoginRequestExternalLogoutFunction& operator=(
      const LoginRequestExternalLogoutFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.requestExternalLogout",
                             LOGIN_REQUESTEXTERNALLOGOUT)

 protected:
  ~LoginRequestExternalLogoutFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginNotifyExternalLogoutDoneFunction : public ExtensionFunction {
 public:
  LoginNotifyExternalLogoutDoneFunction();

  LoginNotifyExternalLogoutDoneFunction(
      const LoginNotifyExternalLogoutDoneFunction&) = delete;

  LoginNotifyExternalLogoutDoneFunction& operator=(
      const LoginNotifyExternalLogoutDoneFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("login.notifyExternalLogoutDone",
                             LOGIN_NOTIFYEXTERNALLOGOUTDONE)

 protected:
  ~LoginNotifyExternalLogoutDoneFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
