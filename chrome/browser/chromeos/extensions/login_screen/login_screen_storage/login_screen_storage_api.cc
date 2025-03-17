// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_storage/login_screen_storage_api.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "chrome/common/extensions/api/login_screen_storage.h"
#include "chromeos/ash/components/dbus/login_manager/login_screen_storage.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace login_screen_storage = extensions::api::login_screen_storage;

namespace {

const char kPersistentDataKeyPrefix[] = "persistent_data_";
const char kCredentialsKeyPrefix[] = "credentials_";

}  // namespace

namespace extensions {

LoginScreenStorageExtensionFunction::LoginScreenStorageExtensionFunction() =
    default;
LoginScreenStorageExtensionFunction::~LoginScreenStorageExtensionFunction() =
    default;

void LoginScreenStorageExtensionFunction::StoreAndRespond(
    std::vector<std::string> keys,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data) {
  if (keys.empty()) {
    Respond(NoArguments());
    return;
  }

  const std::string key = keys.back();
  keys.pop_back();

  auto dbus_callback =
      base::BindOnce(&LoginScreenStorageExtensionFunction::OnStored, this,
                     std::move(keys), metadata, data);
  ash::SessionManagerClient::Get()->LoginScreenStorageStore(
      key, metadata, data, std::move(dbus_callback));
}

void LoginScreenStorageExtensionFunction::OnStored(
    std::vector<std::string> remaining_keys,
    const login_manager::LoginScreenStorageMetadata& metadata,
    const std::string& data,
    std::optional<std::string> error) {
  if (error) {
    Respond(Error(*std::move(error)));
    return;
  }

  if (remaining_keys.empty()) {
    Respond(NoArguments());
    return;
  }

  StoreAndRespond(std::move(remaining_keys), metadata, data);
}

void LoginScreenStorageExtensionFunction::RetrieveAndRespond(
    const std::string& key) {
  auto dbus_callback =
      base::BindOnce(&LoginScreenStorageExtensionFunction::OnRetrieved, this);
  ash::SessionManagerClient::Get()->LoginScreenStorageRetrieve(
      key, std::move(dbus_callback));
}

void LoginScreenStorageExtensionFunction::OnRetrieved(
    std::optional<std::string> data,
    std::optional<std::string> error) {
  if (error) {
    Respond(Error(*std::move(error)));
    return;
  }

  CHECK(data);
  Respond(WithArguments(*std::move(data)));
}

LoginScreenStorageStorePersistentDataFunction::
    LoginScreenStorageStorePersistentDataFunction() = default;
LoginScreenStorageStorePersistentDataFunction::
    ~LoginScreenStorageStorePersistentDataFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageStorePersistentDataFunction::Run() {
  std::optional<login_screen_storage::StorePersistentData::Params> params =
      login_screen_storage::StorePersistentData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<std::string> keys;
  const std::vector<std::string>& receiver_ids =
      std::move(params->extension_ids);
  for (const auto& receiver_id : receiver_ids) {
    const std::string key = base::StrCat(
        {kPersistentDataKeyPrefix, extension_id(), "_", receiver_id});
    keys.push_back(key);
  }

  login_manager::LoginScreenStorageMetadata metadata;
  metadata.set_clear_on_session_exit(false);

  StoreAndRespond(std::move(keys), std::move(metadata), params->data);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginScreenStorageRetrievePersistentDataFunction::
    LoginScreenStorageRetrievePersistentDataFunction() = default;
LoginScreenStorageRetrievePersistentDataFunction::
    ~LoginScreenStorageRetrievePersistentDataFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageRetrievePersistentDataFunction::Run() {
  std::optional<login_screen_storage::RetrievePersistentData::Params> params =
      login_screen_storage::RetrievePersistentData::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string key = base::StrCat(
      {kPersistentDataKeyPrefix, params->owner_id, "_", extension_id()});

  RetrieveAndRespond(key);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginScreenStorageStoreCredentialsFunction::
    LoginScreenStorageStoreCredentialsFunction() = default;
LoginScreenStorageStoreCredentialsFunction::
    ~LoginScreenStorageStoreCredentialsFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageStoreCredentialsFunction::Run() {
  std::optional<login_screen_storage::StoreCredentials::Params> params =
      login_screen_storage::StoreCredentials::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::vector<std::string> keys;
  std::string key = base::StrCat({kCredentialsKeyPrefix, params->extension_id});
  keys.push_back(key);

  login_manager::LoginScreenStorageMetadata metadata;
  metadata.set_clear_on_session_exit(true);

  StoreAndRespond(std::move(keys), std::move(metadata), params->credentials);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginScreenStorageRetrieveCredentialsFunction::
    LoginScreenStorageRetrieveCredentialsFunction() = default;
LoginScreenStorageRetrieveCredentialsFunction::
    ~LoginScreenStorageRetrieveCredentialsFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageRetrieveCredentialsFunction::Run() {
  std::string key = base::StrCat({kCredentialsKeyPrefix, extension_id()});

  RetrieveAndRespond(key);
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
