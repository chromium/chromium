// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api_lacros.h"

#include <string>

#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace extensions {

namespace api_pki = api::platform_keys_internal;
using SigningScheme = crosapi::mojom::KeystoreSigningScheme;
using SigningAlgorithmName = crosapi::mojom::KeystoreSigningAlgorithmName;
using KeystoreService = crosapi::mojom::KeystoreService;

namespace {
const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";
const char kErrorAlgorithmNotPermittedByCertificate[] =
    "The requested Algorithm is not permitted by the certificate.";
const char kErrorAlgorithmNotSupported[] = "Algorithm not supported.";
const char kErrorInvalidToken[] = "The token is not valid.";

using crosapi::keystore_service_util::kWebCryptoEcdsa;
using crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;

base::Optional<SigningAlgorithmName> SigningAlgorithmNameFromString(
    const std::string& input) {
  if (input == kWebCryptoRsassaPkcs1v15)
    return SigningAlgorithmName::kRsassaPkcs115;
  if (input == kWebCryptoEcdsa)
    return SigningAlgorithmName::kEcdsa;
  return base::nullopt;
}

base::Optional<crosapi::mojom::KeystoreType> KeystoreTypeFromString(
    const std::string& input) {
  if (input == "user")
    return crosapi::mojom::KeystoreType::kUser;
  if (input == "system")
    return crosapi::mojom::KeystoreType::kDevice;
  return base::nullopt;
}

base::Optional<SigningScheme> SigningSchemeFromStrings(
    const std::string& hashing,
    const std::string& signing) {
  if (hashing == "none") {
    if (signing == kWebCryptoRsassaPkcs1v15)
      return SigningScheme::kRsassaPkcs1V15None;
    return base::nullopt;
  }
  if (hashing == "SHA-1") {
    if (signing == kWebCryptoRsassaPkcs1v15)
      return SigningScheme::kRsassaPkcs1V15Sha1;
    if (signing == kWebCryptoEcdsa)
      return SigningScheme::kEcdsaSha1;
  }
  if (hashing == "SHA-256") {
    if (signing == kWebCryptoRsassaPkcs1v15)
      return SigningScheme::kRsassaPkcs1V15Sha256;
    if (signing == kWebCryptoEcdsa)
      return SigningScheme::kEcdsaSha256;
  }
  if (hashing == "SHA-384") {
    if (signing == kWebCryptoRsassaPkcs1v15)
      return SigningScheme::kRsassaPkcs1V15Sha384;
    if (signing == kWebCryptoEcdsa)
      return SigningScheme::kEcdsaSha384;
  }
  if (hashing == "SHA-512") {
    if (signing == kWebCryptoRsassaPkcs1v15)
      return SigningScheme::kRsassaPkcs1V15Sha512;
    if (signing == kWebCryptoEcdsa)
      return SigningScheme::kEcdsaSha512;
  }
  return base::nullopt;
}

}  // namespace

PlatformKeysInternalSelectClientCertificatesFunction::
    ~PlatformKeysInternalSelectClientCertificatesFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysInternalSelectClientCertificatesFunction::Run() {
  return RespondNow(Error(kUnsupportedByAsh));
}

PlatformKeysInternalGetPublicKeyFunction::
    ~PlatformKeysInternalGetPublicKeyFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyFunction::Run() {
  std::unique_ptr<api_pki::GetPublicKey::Params> params(
      api_pki::GetPublicKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (chromeos::LacrosChromeServiceImpl::Get()->GetInterfaceVersion(
          KeystoreService::Uuid_) <
      static_cast<int>(KeystoreService::kGetPublicKeyMinVersion)) {
    return RespondNow(Error(kUnsupportedByAsh));
  }

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));

  base::Optional<SigningAlgorithmName> algorithm_name =
      SigningAlgorithmNameFromString(params->algorithm_name);
  if (!algorithm_name) {
    return RespondNow(Error(kErrorAlgorithmNotPermittedByCertificate));
  }

  auto cb = base::BindOnce(
      &PlatformKeysInternalGetPublicKeyFunction::OnGetPublicKey, this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->GetPublicKey(params->certificate, algorithm_name.value(),
                     std::move(cb));
  return RespondLater();
}

void PlatformKeysInternalGetPublicKeyFunction::OnGetPublicKey(
    ResultPtr result) {
  using Result = crosapi::mojom::GetPublicKeyResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::SUCCESS_RESULT:
      api_pki::GetPublicKey::Results::Algorithm algorithm;
      base::Optional<base::DictionaryValue> dict =
          crosapi::keystore_service_util::DictionaryFromSigningAlgorithm(
              result->get_success_result()->algorithm_properties);
      if (!dict) {
        Respond(Error(kUnsupportedByAsh));
        return;
      }
      algorithm.additional_properties = std::move(dict.value());
      Respond(ArgumentList(api_pki::GetPublicKey::Results::Create(
          result->get_success_result()->public_key, std::move(algorithm))));
      return;
  }
}

PlatformKeysInternalGetPublicKeyBySpkiFunction::
    ~PlatformKeysInternalGetPublicKeyBySpkiFunction() = default;

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyBySpkiFunction::Run() {
  return RespondNow(Error("Not implemented."));
}

PlatformKeysInternalSignFunction::~PlatformKeysInternalSignFunction() {}

ExtensionFunction::ResponseAction PlatformKeysInternalSignFunction::Run() {
  std::unique_ptr<api_pki::Sign::Params> params(
      api_pki::Sign::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (chromeos::LacrosChromeServiceImpl::Get()->GetInterfaceVersion(
          KeystoreService::Uuid_) <
      static_cast<int>(KeystoreService::kSignMinVersion)) {
    return RespondNow(Error(kUnsupportedByAsh));
  }

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));

  base::Optional<crosapi::mojom::KeystoreType> keystore_type =
      KeystoreTypeFromString(params->token_id);
  if (!keystore_type) {
    return RespondNow(Error(kErrorInvalidToken));
  }

  base::Optional<SigningScheme> scheme = SigningSchemeFromStrings(
      params->hash_algorithm_name, params->algorithm_name);
  if (!scheme) {
    return RespondNow(Error(kErrorAlgorithmNotSupported));
  }

  auto cb = base::BindOnce(&PlatformKeysInternalSignFunction::OnSign, this);
  chromeos::LacrosChromeServiceImpl::Get()->keystore_service_remote()->Sign(
      keystore_type.value(), params->public_key, scheme.value(), params->data,
      extension_id(), std::move(cb));
  return RespondLater();
}

void PlatformKeysInternalSignFunction::OnSign(ResultPtr result) {
  using Result = crosapi::mojom::KeystoreBinaryResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::BLOB:
      Respond(ArgumentList(api_pki::Sign::Results::Create(result->get_blob())));
      return;
  }
}

PlatformKeysVerifyTLSServerCertificateFunction::
    ~PlatformKeysVerifyTLSServerCertificateFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysVerifyTLSServerCertificateFunction::Run() {
  return RespondNow(Error("Not implemented."));
}

}  // namespace extensions
