// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_storage/login_screen_storage_api.h"

#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"
#include "chrome/common/extensions/api/login_screen_storage.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"

namespace login_screen_storage = extensions::api::login_screen_storage;

namespace {

const char kPersistentDataKeyPrefix[] = "persistent_data_";
const char kCredentialsKeyPrefix[] = "credentials_";

crosapi::mojom::LoginScreenStorage* GetLoginScreenStorageApi() {
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->login_screen_storage_ash();
}

}  // namespace

namespace extensions {

LoginScreenStorageExtensionFunction::LoginScreenStorageExtensionFunction() =
    default;
LoginScreenStorageExtensionFunction::~LoginScreenStorageExtensionFunction() =
    default;

void LoginScreenStorageExtensionFunction::OnDataStored(
    const std::optional<std::string>& error_message) {
  Respond(error_message.has_value() ? Error(*error_message) : NoArguments());
}

void LoginScreenStorageExtensionFunction::OnDataRetrieved(
    crosapi::mojom::LoginScreenStorageRetrieveResultPtr result) {
  using Result = crosapi::mojom::LoginScreenStorageRetrieveResult;
  switch (result->which()) {
    case Result::Tag::kErrorMessage:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::kData:
      Respond(WithArguments(result->get_data()));
      return;
  }
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

  auto callback = base::BindOnce(
      &LoginScreenStorageStorePersistentDataFunction::OnDataStored, this);

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata->clear_on_session_exit = false;

  GetLoginScreenStorageApi()->Store(std::move(keys), std::move(metadata),
                                    params->data, std::move(callback));
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

  auto callback = base::BindOnce(
      &LoginScreenStorageRetrievePersistentDataFunction::OnDataRetrieved, this);

  GetLoginScreenStorageApi()->Retrieve(key, std::move(callback));
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

  auto callback = base::BindOnce(
      &LoginScreenStorageStoreCredentialsFunction::OnDataStored, this);

  crosapi::mojom::LoginScreenStorageMetadataPtr metadata =
      crosapi::mojom::LoginScreenStorageMetadata::New();
  metadata->clear_on_session_exit = true;

  GetLoginScreenStorageApi()->Store(std::move(keys), std::move(metadata),
                                    params->credentials, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginScreenStorageRetrieveCredentialsFunction::
    LoginScreenStorageRetrieveCredentialsFunction() = default;
LoginScreenStorageRetrieveCredentialsFunction::
    ~LoginScreenStorageRetrieveCredentialsFunction() = default;

ExtensionFunction::ResponseAction
LoginScreenStorageRetrieveCredentialsFunction::Run() {
  std::string key = base::StrCat({kCredentialsKeyPrefix, extension_id()});

  auto callback = base::BindOnce(
      &LoginScreenStorageRetrieveCredentialsFunction::OnDataRetrieved, this);

  GetLoginScreenStorageApi()->Retrieve(key, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
