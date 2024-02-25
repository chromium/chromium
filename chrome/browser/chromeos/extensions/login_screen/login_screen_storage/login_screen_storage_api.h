// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_STORAGE_LOGIN_SCREEN_STORAGE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_STORAGE_LOGIN_SCREEN_STORAGE_API_H_

#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Provides common callback functions to return results from the
// LoginScreenStorage crosapi's `Store` and `Retrieve` methods.
class LoginScreenStorageExtensionFunction : public ExtensionFunction {
 public:
  LoginScreenStorageExtensionFunction(
      const LoginScreenStorageExtensionFunction&) = delete;
  LoginScreenStorageExtensionFunction& operator=(
      const LoginScreenStorageExtensionFunction&) = delete;

 protected:
  LoginScreenStorageExtensionFunction();
  ~LoginScreenStorageExtensionFunction() override;

  // When passed as a callback to the LoginScreenStorage `Store` crosapi method,
  // returns its result to the calling extension.
  void OnDataStored(const std::optional<std::string>& error_message);

  // When passed as a callback to the LoginScreenStorage `Retrieve` crosapi
  // method, returns its result to the calling extension.
  void OnDataRetrieved(
      crosapi::mojom::LoginScreenStorageRetrieveResultPtr result);
};

class LoginScreenStorageStorePersistentDataFunction
    : public LoginScreenStorageExtensionFunction {
 public:
  LoginScreenStorageStorePersistentDataFunction();

  LoginScreenStorageStorePersistentDataFunction(
      const LoginScreenStorageStorePersistentDataFunction&) = delete;
  LoginScreenStorageStorePersistentDataFunction& operator=(
      const LoginScreenStorageStorePersistentDataFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("loginScreenStorage.storePersistentData",
                             LOGINSCREENSTORAGE_STOREPERSISTENTDATA)

 protected:
  ~LoginScreenStorageStorePersistentDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginScreenStorageRetrievePersistentDataFunction
    : public LoginScreenStorageExtensionFunction {
 public:
  LoginScreenStorageRetrievePersistentDataFunction();

  LoginScreenStorageRetrievePersistentDataFunction(
      const LoginScreenStorageRetrievePersistentDataFunction&) = delete;
  LoginScreenStorageRetrievePersistentDataFunction& operator=(
      const LoginScreenStorageRetrievePersistentDataFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("loginScreenStorage.retrievePersistentData",
                             LOGINSCREENSTORAGE_RETRIEVEPERSISTENTDATA)

 protected:
  ~LoginScreenStorageRetrievePersistentDataFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginScreenStorageStoreCredentialsFunction
    : public LoginScreenStorageExtensionFunction {
 public:
  LoginScreenStorageStoreCredentialsFunction();

  LoginScreenStorageStoreCredentialsFunction(
      const LoginScreenStorageStoreCredentialsFunction&) = delete;
  LoginScreenStorageStoreCredentialsFunction& operator=(
      const LoginScreenStorageStoreCredentialsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("loginScreenStorage.storeCredentials",
                             LOGINSCREENSTORAGE_STORECREDENTIALS)

 protected:
  ~LoginScreenStorageStoreCredentialsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class LoginScreenStorageRetrieveCredentialsFunction
    : public LoginScreenStorageExtensionFunction {
 public:
  LoginScreenStorageRetrieveCredentialsFunction();

  LoginScreenStorageRetrieveCredentialsFunction(
      const LoginScreenStorageRetrieveCredentialsFunction&) = delete;
  LoginScreenStorageRetrieveCredentialsFunction& operator=(
      const LoginScreenStorageRetrieveCredentialsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("loginScreenStorage.retrieveCredentials",
                             LOGINSCREENSTORAGE_RETRIEVECREDENTIALS)

 protected:
  ~LoginScreenStorageRetrieveCredentialsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_STORAGE_LOGIN_SCREEN_STORAGE_API_H_
