// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_STORAGE_LOGIN_SCREEN_STORAGE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_LOGIN_SCREEN_LOGIN_SCREEN_STORAGE_LOGIN_SCREEN_STORAGE_API_H_

#include "chromeos/dbus/login_manager/login_screen_storage.pb.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Provides common callback functions to return results from
// 'LoginScreenStorageStore' and 'LoginScreenStorageRetrieve' D-Bus methods.
class LoginScreenStorageExtensionFunction : public ExtensionFunction {
 public:
  LoginScreenStorageExtensionFunction(
      const LoginScreenStorageExtensionFunction&) = delete;
  LoginScreenStorageExtensionFunction& operator=(
      const LoginScreenStorageExtensionFunction&) = delete;

 protected:
  LoginScreenStorageExtensionFunction();
  ~LoginScreenStorageExtensionFunction() override;

  // When passed as a callback to the 'LoginScreenStorageStore' D-Bus method,
  // returns its result to the calling extension.
  void OnDataStored(absl::optional<std::string> error);

  // When passed as a callback to the 'LoginScreenStorageRetrieve' D-Bus method,
  // returns its result to the calling extension.
  void OnDataRetrieved(absl::optional<std::string> data,
                       absl::optional<std::string> error);
};

class LoginScreenStorageStorePersistentDataFunction : public ExtensionFunction {
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

 private:
  // Called when data for one of the extension was stored, |extension_ids| is a
  // list of the extensions that the data wasn't yet stored for.
  void OnDataStored(std::vector<std::string> extension_ids,
                    const login_manager::LoginScreenStorageMetadata& metadata,
                    const std::string& data,
                    absl::optional<std::string> error);

  // Asynchronously stores data for every extension from |extension_ids|.
  void StoreDataForExtensions(
      std::vector<std::string> extension_ids,
      const login_manager::LoginScreenStorageMetadata& metadata,
      const std::string& data);
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
