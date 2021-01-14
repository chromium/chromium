// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api_lacros.h"

#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace extensions {

namespace api_pki = api::platform_keys_internal;
using SigningAlgorithmName = crosapi::mojom::KeystoreSigningAlgorithmName;

namespace {
const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";
const char kErrorAlgorithmNotPermittedByCertificate[] =
    "The requested Algorithm is not permitted by the certificate.";

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
          crosapi::mojom::KeystoreService::Uuid_) < 3) {
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
  return RespondNow(Error("Not implemented."));
}

PlatformKeysVerifyTLSServerCertificateFunction::
    ~PlatformKeysVerifyTLSServerCertificateFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysVerifyTLSServerCertificateFunction::Run() {
  return RespondNow(Error("Not implemented."));
}

}  // namespace extensions
