// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_

#include "components/prefs/pref_registry_simple.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

}  // namespace login_api

namespace login_api_errors {

extern const char kAlreadyActiveSession[];
extern const char kLoginScreenIsNotActive[];
extern const char kAnotherLoginAttemptInProgress[];
extern const char kNoManagedGuestSessionAccounts[];
extern const char kNoPermissionToLock[];
extern const char kSessionIsNotActive[];
extern const char kNoPermissionToUnlock[];
extern const char kSessionIsNotLocked[];
extern const char kAnotherUnlockAttemptInProgress[];
extern const char kAuthenticationFailed[];
extern const char kSharedMGSAlreadyLaunched[];
extern const char kNoSharedMGSFound[];
extern const char kSharedSessionIsNotActive[];
extern const char kSharedSessionAlreadyLaunched[];
extern const char kScryptFailure[];
extern const char kCleanupInProgress[];
extern const char kUnlockFailure[];
extern const char kNoPermissionToUseApi[];

}  // namespace login_api_errors

class LoginLaunchManagedGuestSessionFunction : public ExtensionFunction {
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

class LoginExitCurrentSessionFunction : public ExtensionFunction {
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

class LoginFetchDataForNextLoginAttemptFunction : public ExtensionFunction {
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

class LoginLockManagedGuestSessionFunction : public ExtensionFunction {
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

class LoginUnlockManagedGuestSessionFunction : public ExtensionFunction {
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

 private:
  void OnAuthenticationComplete(bool success);
};

class LoginLaunchSharedManagedGuestSessionFunction : public ExtensionFunction {
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

class LoginEnterSharedSessionFunction : public ExtensionFunction {
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

 private:
  void OnEnterSharedSessionComplete(absl::optional<std::string> error);
};

class LoginUnlockSharedSessionFunction : public ExtensionFunction {
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

 private:
  void OnUnlockSharedSessionComplete(absl::optional<std::string> error);
};

class LoginEndSharedSessionFunction : public ExtensionFunction {
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

 private:
  void OnEndSharedSessionComplete(absl::optional<std::string> error);
};

class LoginSetDataForNextLoginAttemptFunction : public ExtensionFunction {
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

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
