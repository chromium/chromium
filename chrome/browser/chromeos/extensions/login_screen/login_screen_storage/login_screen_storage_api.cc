// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_screen_storage/login_screen_storage_api.h"

#include "base/strings/strcat.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/extensions/api/login_screen_storage.h"
#include "chromeos/crosapi/mojom/login_screen_storage.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_screen_storage_ash.h"
#endif

namespace login_screen_storage = extensions::api::login_screen_storage;

namespace {

const char kPersistentDataKeyPrefix[] = "persistent_data_";
const char kCredentialsKeyPrefix[] = "credentials_";

crosapi::mojom::LoginScreenStorage* GetLoginScreenStorageApi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::LoginScreenStorage>()
      .get();
#else
  return crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->login_screen_storage_ash();
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kUnsupportedByAsh[] = "Not supported by ash.";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or nullopt on success.
std::optional<std::string> ValidateCrosapi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::LoginScreenStorage>()) {
    return kUnsupportedByAsh;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  return std::nullopt;
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

  std::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

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

  std::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

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

  std::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

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
  std::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }

  std::string key = base::StrCat({kCredentialsKeyPrefix, extension_id()});

  auto callback = base::BindOnce(
      &LoginScreenStorageRetrieveCredentialsFunction::OnDataRetrieved, this);

  GetLoginScreenStorageApi()->Retrieve(key, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace extensions
