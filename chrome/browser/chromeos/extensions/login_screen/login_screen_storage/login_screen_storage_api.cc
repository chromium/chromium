// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_storage/login_screen_storage_api.h"

#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/common/extensions/api/login_screen_storage.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/user_manager/user_manager.h"

namespace login_screen_storage = extensions::api::login_screen_storage;

namespace extensions {

namespace {

const char kPersistentDataKeyPrefix[] = "persistent_data_";
const char kCredentialsKeyPrefix[] = "credentials_";

}  // namespace

LoginScreenStorageExtensionFunction::LoginScreenStorageExtensionFunction() =
    default;
LoginScreenStorageExtensionFunction::~LoginScreenStorageExtensionFunction() =
    default;

void LoginScreenStorageExtensionFunction::OnDataStored(
    base::Optional<std::string> error) {
  Respond(error ? Error(*error) : NoArguments());
}

void LoginScreenStorageExtensionFunction::OnDataRetrieved(
    base::Optional<std::string> data,
    base::Optional<std::string> error) {
  if (error) {
    Respond(Error(*error));
    return;
  }
  Respond(OneArgument(data ? std::make_unique<base::Value>(*data) : nullptr));
}

LoginScreenStorageStorePersistentDataFunction::
    LoginScreenStorageStorePersistentDataFunction() = default;
LoginScreenStorageStorePersistentDataFunction::
    ~LoginScreenStorageStorePersistentDataFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageStorePersistentDataFunction::Run() {
  std::unique_ptr<login_screen_storage::StorePersistentData::Params> params =
      login_screen_storage::StorePersistentData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  login_manager::LoginScreenStorageMetadata metadata;
  metadata.set_clear_on_session_exit(false);
  StoreDataForExtensions(std::move(params->extension_ids), metadata,
                         params->data);
  return RespondLater();
}

void LoginScreenStorageStorePersistentDataFunction::OnDataStored(
    std::vector<std::string> extension_ids,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data,
    base::Optional<std::string> error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  if (extension_ids.empty()) {
    Respond(NoArguments());
    return;
  }

  StoreDataForExtensions(std::move(extension_ids), metadata, data);
}

void LoginScreenStorageStorePersistentDataFunction::StoreDataForExtensions(
    std::vector<std::string> extension_ids,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data) {
  if (extension_ids.empty())
    return;

  std::string receiver_id = extension_ids.back();
  extension_ids.pop_back();
  chromeos::SessionManagerClient::Get()->LoginScreenStorageStore(
      kPersistentDataKeyPrefix + extension_id() + "_" + receiver_id, metadata,
      data,
      base::BindOnce(
          &LoginScreenStorageStorePersistentDataFunction::OnDataStored, this,
          std::move(extension_ids), metadata, data));
}

LoginScreenStorageRetrievePersistentDataFunction::
    LoginScreenStorageRetrievePersistentDataFunction() = default;
LoginScreenStorageRetrievePersistentDataFunction::
    ~LoginScreenStorageRetrievePersistentDataFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageRetrievePersistentDataFunction::Run() {
  std::unique_ptr<login_screen_storage::RetrievePersistentData::Params> params =
      login_screen_storage::RetrievePersistentData::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::SessionManagerClient::Get()->LoginScreenStorageRetrieve(
      base::StrCat(
          {kPersistentDataKeyPrefix, params->owner_id, "_", extension_id()}),
      base::BindOnce(
          &LoginScreenStorageRetrievePersistentDataFunction::OnDataRetrieved,
          this));
  return RespondLater();
}

LoginScreenStorageStoreCredentialsFunction::
    LoginScreenStorageStoreCredentialsFunction() = default;
LoginScreenStorageStoreCredentialsFunction::
    ~LoginScreenStorageStoreCredentialsFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageStoreCredentialsFunction::Run() {
  std::unique_ptr<login_screen_storage::StoreCredentials::Params> params =
      login_screen_storage::StoreCredentials::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params);
  login_manager::LoginScreenStorageMetadata metadata;
  metadata.set_clear_on_session_exit(true);
  chromeos::SessionManagerClient::Get()->LoginScreenStorageStore(
      kCredentialsKeyPrefix + params->extension_id, metadata,
      params->credentials,
      base::BindOnce(&LoginScreenStorageStoreCredentialsFunction::OnDataStored,
                     this));
  return RespondLater();
}

LoginScreenStorageRetrieveCredentialsFunction::
    LoginScreenStorageRetrieveCredentialsFunction() = default;
LoginScreenStorageRetrieveCredentialsFunction::
    ~LoginScreenStorageRetrieveCredentialsFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageRetrieveCredentialsFunction::Run() {
  chromeos::SessionManagerClient::Get()->LoginScreenStorageRetrieve(
      kCredentialsKeyPrefix + extension_id(),
      base::BindOnce(
          &LoginScreenStorageRetrieveCredentialsFunction::OnDataRetrieved,
          this));
  return RespondLater();
}

}  // namespace extensions
