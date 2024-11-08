// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/extensions/api/platform_keys_core/platform_keys_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;
using crosapi::mojom::KeystoreService;

const char kExtensionDoesNotHavePermission[] =
    "The extension does not have permission to call this function.";

crosapi::mojom::KeystoreService* GetKeystoreService(
    content::BrowserContext* browser_context) {
  return crosapi::KeystoreServiceFactoryAsh::GetForBrowserContext(
      browser_context);
}

std::optional<crosapi::mojom::KeystoreType> KeystoreTypeFromString(
    const std::string& input) {
  if (input == "user") {
    return crosapi::mojom::KeystoreType::kUser;
  }
  if (input == "system") {
    return crosapi::mojom::KeystoreType::kDevice;
  }
  return std::nullopt;
}

// Validates that |token_id| is well-formed. Converts |token_id| into the output
// parameter |keystore|. Only populated on success. Returns an empty string on
// success and an error message on error. A validation error should result in
// extension termination.
std::string ValidateInput(const std::string& token_id,
                          crosapi::mojom::KeystoreType* keystore) {
  std::optional<crosapi::mojom::KeystoreType> keystore_type =
      KeystoreTypeFromString(token_id);
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

  if (params->algorithm.name == "RSASSA-PKCS1-v1_5") {
    // TODO(pneubeck): Add support for unsigned integers to IDL.
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.modulus_length &&
                                *(params->algorithm.modulus_length) >= 0);
    service->GenerateRSAKey(
        platform_keys_token_id.value(), *(params->algorithm.modulus_length),
        params->software_backed, extension_id(),
        base::BindOnce(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else if (params->algorithm.name == "ECDSA") {
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.named_curve);
    service->GenerateECKey(
        platform_keys_token_id.value(), *(params->algorithm.named_curve),
        extension_id(),
        base::BindOnce(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else {
    NOTREACHED();
  }
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    std::vector<uint8_t> public_key_der,
    std::optional<crosapi::mojom::KeystoreError> error) {
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

  crosapi::mojom::KeystoreType keystore;
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
    crosapi::mojom::GetCertificatesResultPtr result) {
  if (result->is_error()) {
    Respond(Error(
        chromeos::platform_keys::KeystoreErrorToString(result->get_error())));
    return;
  }
  DCHECK(result->is_certificates());

  base::Value::List client_certs;
  for (std::vector<uint8_t>& cert : result->get_certificates()) {
    client_certs.Append(base::Value(std::move(cert)));
  }

  base::Value::List results;
  results.Append(std::move(client_certs));
  Respond(ArgumentList(std::move(results)));
}

//------------------------------------------------------------------------------

ExtensionFunction::ResponseAction
EnterprisePlatformKeysImportCertificateFunction::Run() {
  std::optional<api_epk::ImportCertificate::Params> params =
      api_epk::ImportCertificate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  crosapi::mojom::KeystoreType keystore;
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
    crosapi::mojom::KeystoreError error_code) {
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

  crosapi::mojom::KeystoreType keystore;
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
    crosapi::mojom::KeystoreError error) {
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
    crosapi::mojom::GetKeyStoresResultPtr result) {
  if (result->is_error()) {
    Respond(Error(
        chromeos::platform_keys::KeystoreErrorToString(result->get_error())));
    return;
  }
  DCHECK(result->is_key_stores());

  std::vector<std::string> key_stores;
  using KeystoreType = crosapi::mojom::KeystoreType;
  for (KeystoreType keystore_type : result->get_key_stores()) {
    if (!crosapi::mojom::IsKnownEnumValue(keystore_type)) {
      continue;
    }

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
          crosapi::mojom::KeystoreType::kDevice, params->challenge,
          /*migrate=*/params->register_key ? *params->register_key : false,
          crosapi::mojom::KeystoreAlgorithmName::kRsassaPkcs115, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        crosapi::mojom::ChallengeAttestationOnlyKeystoreResultPtr result) {
  // It's possible the browser context was destroyed during the async work.
  // If that happens, bail.
  if (!browser_context()) {
    return;
  }
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_challenge_response());

  Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
      result->get_challenge_response())));
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
          crosapi::mojom::KeystoreType::kUser, params->challenge,
          /*migrate=*/params->register_key,
          crosapi::mojom::KeystoreAlgorithmName::kRsassaPkcs115, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::
    OnChallengeAttestationOnlyKeystore(
        crosapi::mojom::ChallengeAttestationOnlyKeystoreResultPtr result) {
  // It's possible the browser context was destroyed during the async work.
  // If that happens, bail.
  if (!browser_context()) {
    return;
  }
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_challenge_response());
  Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
      result->get_challenge_response())));
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

  crosapi::mojom::KeystoreType keystore_type =
      crosapi::mojom::KeystoreType::kDevice;
  EXTENSION_FUNCTION_VALIDATE(params->options.scope !=
                              api::enterprise_platform_keys::Scope::kNone);
  switch (params->options.scope) {
    case api::enterprise_platform_keys::Scope::kUser:
      keystore_type = crosapi::mojom::KeystoreType::kUser;
      break;
    case api::enterprise_platform_keys::Scope::kMachine:
      keystore_type = crosapi::mojom::KeystoreType::kDevice;
      break;
    case api::enterprise_platform_keys::Scope::kNone:
      NOTREACHED();
  }

  // Default to RSA when not registering a key.
  crosapi::mojom::KeystoreAlgorithmName algorithm =
      crosapi::mojom::KeystoreAlgorithmName::kRsassaPkcs115;
  if (params->options.register_key.has_value()) {
    EXTENSION_FUNCTION_VALIDATE(
        params->options.register_key->algorithm !=
        api::enterprise_platform_keys::Algorithm::kNone);
    switch (params->options.register_key->algorithm) {
      case api::enterprise_platform_keys::Algorithm::kRsa:
        algorithm = crosapi::mojom::KeystoreAlgorithmName::kRsassaPkcs115;
        break;
      case api::enterprise_platform_keys::Algorithm::kEcdsa: {
        algorithm = crosapi::mojom::KeystoreAlgorithmName::kEcdsa;
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
        crosapi::mojom::ChallengeAttestationOnlyKeystoreResultPtr result) {
  // It's possible the browser context was destroyed during the async work.
  // If that happens, bail.
  if (!browser_context()) {
    return;
  }
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }
  DCHECK(result->is_challenge_response());
  Respond(ArgumentList(api_epk::ChallengeKey::Results::Create(
      result->get_challenge_response())));
}

}  // namespace extensions
