// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ash/platform_keys/keystore_service.h"
#include "chrome/browser/ash/platform_keys/keystore_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chromeos/ash/components/platform_keys/keystore_types.h"
#include "chromeos/ash/components/platform_keys/platform_keys.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;
using chromeos::KeystoreError;
using chromeos::KeystoreType;

const char kExtensionDoesNotHavePermission[] =
    "The extension does not have permission to call this function.";

ash::KeystoreService* GetKeystoreService(
    content::BrowserContext* browser_context) {
  return ash::KeystoreServiceFactory::GetForBrowserContext(browser_context);
}

std::optional<KeystoreType> KeystoreTypeFromString(const std::string& input) {
  if (input == "user") {
    return KeystoreType::kUser;
  }
  if (input == "system") {
    return KeystoreType::kDevice;
  }
  return std::nullopt;
}

std::optional<chromeos::platform_keys::KeyType> KeyTypeFromString(
    const std::string& input) {
  if (input == "RSASSA-PKCS1-v1_5") {
    return chromeos::platform_keys::KeyType::kRsassaPkcs1V15;
  }
  if (input == "RSA-OAEP") {
    return chromeos::platform_keys::KeyType::kRsaOaep;
  }
  if (input == "ECDSA") {
    return chromeos::platform_keys::KeyType::kEcdsa;
  }
  return std::nullopt;
}

// Validates that |token_id| is well-formed. Converts |token_id| into the output
// parameter |keystore|. Only populated on success. Returns an empty string on
// success and an error message on error. A validation error should result in
// extension termination.
std::string ValidateInput(const std::string& token_id, KeystoreType* keystore) {
  std::optional<KeystoreType> keystore_type = KeystoreTypeFromString(token_id);
  if (!keystore_type) {
    return platform_keys::kErrorInvalidToken;
  }

  *keystore = keystore_type.value();
  return "";
}
}  // namespace

//------------------------------------------------------------------------------

EnterprisePlatformKeysInternalGenerateKeyFunction::
    ~EnterprisePlatformKeysInternalGenerateKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGenerateKeyFunction::Run() {
  std::optional<api_epki::GenerateKey::Params> params =
      api_epki::GenerateKey::Params::Create(args());

  EXTENSION_FUNCTION_VALIDATE(params);
  std::optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id) {
    return RespondNow(Error(platform_keys::kErrorInvalidToken));
  }

  chromeos::ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  std::optional<chromeos::platform_keys::KeyType> key_type =
      KeyTypeFromString(params->algorithm.name);
  CHECK(key_type.has_value());

  switch (*key_type) {
    case chromeos::platform_keys::KeyType::kRsassaPkcs1V15:
    case chromeos::platform_keys::KeyType::kRsaOaep:
      // TODO(pneubeck): Add support for unsigned integers to IDL.
      EXTENSION_FUNCTION_VALIDATE(params->algorithm.modulus_length &&
                                  *(params->algorithm.modulus_length) >= 0);
      service->GenerateRSAKey(
          platform_keys_token_id.value(), *key_type,
          *(params->algorithm.modulus_length), params->software_backed,
          extension_id(),
          base::BindOnce(&EnterprisePlatformKeysInternalGenerateKeyFunction::
                             OnGeneratedKey,
                         this));
      break;
    case chromeos::platform_keys::KeyType::kEcdsa:
      EXTENSION_FUNCTION_VALIDATE(params->algorithm.named_curve);
      service->GenerateECKey(
          platform_keys_token_id.value(), *key_type,
          *(params->algorithm.named_curve), extension_id(),
          base::BindOnce(&EnterprisePlatformKeysInternalGenerateKeyFunction::
                             OnGeneratedKey,
                         this));
      break;
  }

  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    std::vector<uint8_t> public_key_der,
    std::optional<chromeos::KeystoreError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error) {
    Respond(ArgumentList(
        api_epki::GenerateKey::Results::Create(std::move(public_key_der))));
  } else {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(error.value())));
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysGetCertificatesFunction::Run() {
  std::optional<api_epk::GetCertificates::Params> params =
      api_epk::GetCertificates::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  KeystoreType keystore;
  std::string error = ValidateInput(params->token_id, &keystore);
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  auto c = base::BindOnce(
      &EnterprisePlatformKeysGetCertificatesFunction::OnGetCertificates, this);
  GetKeystoreService(browser_context())
      ->GetCertificates(keystore, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysGetCertificatesFunction::OnGetCertificates(
    chromeos::GetCertificatesResult result) {
  if (!result.has_value()) {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(result.error())));
    return;
  }

  base::ListValue client_certs;
  for (std::vector<uint8_t>& cert : *result) {
    client_certs.Append(base::Value(std::move(cert)));
  }

  base::ListValue results;
  results.Append(std::move(client_certs));
  Respond(ArgumentList(std::move(results)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysImportCertificateFunction::Run() {
  std::optional<api_epk::ImportCertificate::Params> params =
      api_epk::ImportCertificate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  KeystoreType keystore;
  std::string error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysImportCertificateFunction::OnAddCertificate, this);
  GetKeystoreService(browser_context())
      ->AddCertificate(keystore, params->certificate, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysImportCertificateFunction::OnAddCertificate(
    bool is_error,
    chromeos::KeystoreError error_code) {
  if (!is_error) {
    Respond(NoArguments());
  } else {
    Respond(Error(chromeos::platform_keys::KeystoreErrorToString(error_code)));
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysRemoveCertificateFunction::Run() {
  std::optional<api_epk::RemoveCertificate::Params> params =
      api_epk::RemoveCertificate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  KeystoreType keystore;
  std::string error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysRemoveCertificateFunction::OnRemoveCertificate,
      this);
  GetKeystoreService(browser_context())
      ->RemoveCertificate(keystore, params->certificate, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysRemoveCertificateFunction::OnRemoveCertificate(
    bool is_error,
    chromeos::KeystoreError error) {
  if (!is_error) {
    Respond(NoArguments());
  } else {
    Respond(Error(chromeos::platform_keys::KeystoreErrorToString(error)));
  }
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGetTokensFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysInternalGetTokensFunction::OnGetKeyStores, this);
  GetKeystoreService(browser_context())->GetKeyStores(std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGetTokensFunction::OnGetKeyStores(
    chromeos::GetKeyStoresResult result) {
  if (!result.has_value()) {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(result.error())));
    return;
  }

  std::vector<std::string> key_stores;
  for (KeystoreType keystore_type : *result) {
    switch (keystore_type) {
      case KeystoreType::kUser:
        key_stores.push_back("user");
        break;
      case KeystoreType::kDevice:
        key_stores.push_back("system");
        break;
    }
  }
  Respond(ArgumentList(api_epki::GetTokens::Results::Create(key_stores)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::optional<api_epk::ChallengeMachineKey::Params> params =
      api_epk::ChallengeMachineKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeMachineKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  GetKeystoreService(browser_context())
      ->ChallengeAttestationOnlyKeystore(
          KeystoreType::kDevice, params->challenge,
          /*migrate=*/params->register_key ? *params->register_key : false,
          chromeos::KeystoreAlgorithmName::kRsassaPkcs115, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        chromeos::ChallengeAttestationOnlyKeystoreResult result) {
  // It's possible the browser context was destroyed during the async work.
  // If that happens, bail.
  if (!browser_context()) {
    return;
  }
  if (!result.has_value()) {
    Respond(Error(result.error()));
    return;
  }
  Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(*result)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeUserKeyFunction::Run() {
  std::optional<api_epk::ChallengeUserKey::Params> params =
      api_epk::ChallengeUserKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeUserKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  GetKeystoreService(browser_context())
      ->ChallengeAttestationOnlyKeystore(
          KeystoreType::kUser, params->challenge,
          /*migrate=*/params->register_key,
          chromeos::KeystoreAlgorithmName::kRsassaPkcs115, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        chromeos::ChallengeAttestationOnlyKeystoreResult result) {
  // It's possible the browser context was destroyed during the async work.
  // If that happens, bail.
  if (!browser_context()) {
    return;
  }
  if (!result.has_value()) {
    Respond(Error(result.error()));
    return;
  }
  Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(*result)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeKeyFunction::Run() {
  std::optional<api_epk::ChallengeKey::Params> params =
      api_epk::ChallengeKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  KeystoreType keystore_type = KeystoreType::kDevice;
  EXTENSION_FUNCTION_VALIDATE(params->options.scope !=
                              api::enterprise_platform_keys::Scope::kNone);
  switch (params->options.scope) {
    case api::enterprise_platform_keys::Scope::kUser:
      keystore_type = KeystoreType::kUser;
      break;
    case api::enterprise_platform_keys::Scope::kMachine:
      keystore_type = KeystoreType::kDevice;
      break;
    case api::enterprise_platform_keys::Scope::kNone:
      NOTREACHED();
  }

  // Default to RSA when not registering a key.
  chromeos::KeystoreAlgorithmName algorithm =
      chromeos::KeystoreAlgorithmName::kRsassaPkcs115;
  if (params->options.register_key.has_value()) {
    EXTENSION_FUNCTION_VALIDATE(
        params->options.register_key->algorithm !=
        api::enterprise_platform_keys::Algorithm::kNone);
    switch (params->options.register_key->algorithm) {
      case api::enterprise_platform_keys::Algorithm::kRsa:
        algorithm = chromeos::KeystoreAlgorithmName::kRsassaPkcs115;
        break;
      case api::enterprise_platform_keys::Algorithm::kEcdsa: {
        algorithm = chromeos::KeystoreAlgorithmName::kEcdsa;
        break;
      }
      case api::enterprise_platform_keys::Algorithm::kNone:
        NOTREACHED();
    }
  }

  auto callback = base::BindOnce(&EnterprisePlatformKeysChallengeKeyFunction::
                                     OnChallengeAttestationOnlyKeystore,
                                 this);
  GetKeystoreService(browser_context())
      ->ChallengeAttestationOnlyKeystore(
          keystore_type, params->options.challenge,
          /*migrate=*/params->options.register_key.has_value(), algorithm,
          std::move(callback));

  return RespondLater();
}

void EnterprisePlatformKeysChallengeKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        chromeos::ChallengeAttestationOnlyKeystoreResult result) {
  // It's possible the browser context was destroyed during the async work.
  // If that happens, bail.
  if (!browser_context()) {
    return;
  }
  if (!result.has_value()) {
    Respond(Error(result.error()));
    return;
  }
  Respond(ArgumentList(api_epk::ChallengeKey::Results::Create(*result)));
}

}  // namespace extensions
