// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"

#include <string>

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

}  // namespace extensions
