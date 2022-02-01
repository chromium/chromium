// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_

#include "chromeos/crosapi/mojom/login.mojom.h"
#include "extensions/browser/extension_function.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class ExtensionFunctionWithOptionalErrorResult : public ExtensionFunction {
 protected:
  ~ExtensionFunctionWithOptionalErrorResult() override;

  void OnResult(const absl::optional<std::string>& error);
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

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_LOGIN_API_H_
