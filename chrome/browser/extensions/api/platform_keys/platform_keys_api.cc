// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"

#include <string>

#include "chrome/browser/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/platform_keys/platform_keys.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom-shared.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

namespace api_pki = api::platform_keys_internal;
using crosapi::mojom::KeystoreService;
using SigningAlgorithmName = crosapi::mojom::KeystoreSigningAlgorithmName;
using crosapi::keystore_service_util::kWebCryptoEcdsa;
using crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
const char kErrorInvalidSigningAlgorithm[] = "Invalid signing algorithm.";

const char kTokenIdUser[] = "user";
const char kTokenIdSystem[] = "system";

crosapi::mojom::KeystoreService* GetKeystoreService(
    content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  CHECK(Profile::FromBrowserContext(browser_context)->IsMainProfile())
      << "Attempted to use an incorrect profile. Please file a bug at "
         "https://bugs.chromium.org/ if this happens.";
  return chromeos::LacrosService::Get()->GetRemote<KeystoreService>().get();
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return crosapi::KeystoreServiceFactoryAsh::GetForBrowserContext(
      browser_context);
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)
}

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or empty string on success. |min_version| is the minimum version of
// the ash implementation of KeystoreService necessary to support this
// extension. |context| is the browser context in which the extension is hosted.
std::string ValidateCrosapi(int min_version, content::BrowserContext* context) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  int version = chromeos::LacrosService::Get()->GetInterfaceVersion(
      KeystoreService::Uuid_);
  if (version < min_version)
    return kUnsupportedByAsh;

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile())
    return kUnsupportedProfile;
#endif  // #if BUILDFLAG(IS_CHROMEOS_LACROS)

  return "";
}

absl::optional<SigningAlgorithmName> SigningAlgorithmNameFromString(
    const std::string& input) {
  if (input == kWebCryptoRsassaPkcs1v15)
    return SigningAlgorithmName::kRsassaPkcs115;
  if (input == kWebCryptoEcdsa)
    return SigningAlgorithmName::kEcdsa;
  return absl::nullopt;
}

}  // namespace

namespace platform_keys {

const char kErrorInvalidToken[] = "The token is not valid.";

absl::optional<chromeos::platform_keys::TokenId> ApiIdToPlatformKeysTokenId(
    const std::string& token_id) {
  if (token_id == kTokenIdUser)
    return chromeos::platform_keys::TokenId::kUser;

  if (token_id == kTokenIdSystem)
    return chromeos::platform_keys::TokenId::kSystem;

  return absl::nullopt;
}

}  // namespace platform_keys

//------------------------------------------------------------------------------

PlatformKeysInternalGetPublicKeyFunction::
    ~PlatformKeysInternalGetPublicKeyFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyFunction::Run() {
  std::unique_ptr<api_pki::GetPublicKey::Params> params(
      api_pki::GetPublicKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(KeystoreService::kGetPublicKeyMinVersion,
                                      browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  absl::optional<SigningAlgorithmName> algorithm_name =
      SigningAlgorithmNameFromString(params->algorithm_name);
  if (!algorithm_name) {
    return RespondNow(Error(chromeos::platform_keys::KeystoreErrorToString(
        crosapi::mojom::KeystoreError::kAlgorithmNotSupported)));
  }

  auto cb = base::BindOnce(
      &PlatformKeysInternalGetPublicKeyFunction::OnGetPublicKey, this);
  GetKeystoreService(browser_context())
      ->GetPublicKey(params->certificate, algorithm_name.value(),
                     std::move(cb));
  return RespondLater();
}

void PlatformKeysInternalGetPublicKeyFunction::OnGetPublicKey(
    crosapi::mojom::GetPublicKeyResultPtr result) {
  if (result->is_error_message()) {
    Respond(Error(result->get_error_message()));
    return;
  }

  api_pki::GetPublicKey::Results::Algorithm algorithm;
  absl::optional<base::DictionaryValue> dict =
      crosapi::keystore_service_util::DictionaryFromSigningAlgorithm(
          result->get_success_result()->algorithm_properties);
  if (!dict) {
    Respond(Error(kErrorInvalidSigningAlgorithm));
    return;
  }
  algorithm.additional_properties = std::move(dict.value());
  Respond(ArgumentList(api_pki::GetPublicKey::Results::Create(
      result->get_success_result()->public_key, std::move(algorithm))));
}

//------------------------------------------------------------------------------

PlatformKeysInternalSignFunction::~PlatformKeysInternalSignFunction() {}

ExtensionFunction::ResponseAction PlatformKeysInternalSignFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::unique_ptr<api_pki::Sign::Params> params(
      api_pki::Sign::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  absl::optional<chromeos::platform_keys::TokenId> platform_keys_token_id;
  // If |params->token_id| is not specified (empty string), the key will be
  // searched for in all available tokens.
  if (!params->token_id.empty()) {
    platform_keys_token_id =
        platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
    if (!platform_keys_token_id) {
      return RespondNow(Error(platform_keys::kErrorInvalidToken));
    }
  }

  chromeos::ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  if (params->hash_algorithm_name == "none") {
    // Signing without digesting is only supported for RSASSA-PKCS1-v1_5.
    if (params->algorithm_name != kWebCryptoRsassaPkcs1v15) {
      return RespondNow(Error(StatusToString(
          chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
    }

    service->SignRSAPKCS1Raw(
        platform_keys_token_id,
        std::string(params->data.begin(), params->data.end()),
        std::string(params->public_key.begin(), params->public_key.end()),
        extension_id(),
        base::BindOnce(&PlatformKeysInternalSignFunction::OnSigned, this));
  } else {
    chromeos::platform_keys::HashAlgorithm hash_algorithm;
    if (params->hash_algorithm_name == "SHA-1") {
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA1;
    } else if (params->hash_algorithm_name == "SHA-256") {
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA256;
    } else if (params->hash_algorithm_name == "SHA-384") {
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA384;
    } else if (params->hash_algorithm_name == "SHA-512") {
      hash_algorithm = chromeos::platform_keys::HASH_ALGORITHM_SHA512;
    } else {
      return RespondNow(Error(StatusToString(
          chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
    }

    chromeos::platform_keys::KeyType key_type;
    if (params->algorithm_name == kWebCryptoRsassaPkcs1v15) {
      key_type = chromeos::platform_keys::KeyType::kRsassaPkcs1V15;
    } else if (params->algorithm_name == kWebCryptoEcdsa) {
      key_type = chromeos::platform_keys::KeyType::kEcdsa;
    } else {
      return RespondNow(Error(StatusToString(
          chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
    }

    service->SignDigest(
        platform_keys_token_id,
        std::string(params->data.begin(), params->data.end()),
        std::string(params->public_key.begin(), params->public_key.end()),
        key_type, hash_algorithm, extension_id(),
        base::BindOnce(&PlatformKeysInternalSignFunction::OnSigned, this));
  }

  return RespondLater();
}

void PlatformKeysInternalSignFunction::OnSigned(
    const std::string& signature,
    absl::optional<crosapi::mojom::KeystoreError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!error) {
    Respond(ArgumentList(api_pki::Sign::Results::Create(
        std::vector<uint8_t>(signature.begin(), signature.end()))));
  } else {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(error.value())));
  }
}

}  // namespace extensions
