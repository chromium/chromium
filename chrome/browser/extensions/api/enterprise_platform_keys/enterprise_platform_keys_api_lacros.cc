// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;
using KeystoreService = crosapi::mojom::KeystoreService;

std::vector<uint8_t> VectorFromString(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string StringFromVector(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

base::Optional<crosapi::mojom::KeystoreType> KeystoreTypeFromString(
    const std::string& input) {
  if (input == "user")
    return crosapi::mojom::KeystoreType::kUser;
  if (input == "system")
    return crosapi::mojom::KeystoreType::kDevice;
  return base::nullopt;
}

const char kLacrosNotImplementedError[] = "Not implemented.";
const char kUnsupportedByAsh[] = "Not implemented.";
const char kUnsupportedProfile[] = "Not available.";
const char kUnsupportedSigning[] = "Unsupported signing algorithm.";
const char kUnsupportedSigningInput[] =
    "Unsupported input for signing algorithm.";
const char kInvalidKeystoreType[] = "Invalid keystore type.";
const char kExtensionDoesNotHavePermission[] =
    "The extension does not have permission to call this function.";

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or empty string on success. |min_version| is the minimum version of
// the ash implementation of KeystoreService necessary to support this
// extension. |context| is the browser context in which the extension is hosted.
std::string ValidateCrosapi(int min_version, content::BrowserContext* context) {
  int version = chromeos::LacrosChromeServiceImpl::Get()->GetInterfaceVersion(
      KeystoreService::Uuid_);
  if (version < min_version)
    return kUnsupportedByAsh;

  // These APIs are used in security-sensitive contexts. We need to ensure that
  // the user for ash is the same as the user for lacros. We do this by
  // restricting the API to the default profile, which is guaranteed to be the
  // same user.
  if (!Profile::FromBrowserContext(context)->IsMainProfile())
    return kUnsupportedProfile;

  return "";
}

// Validates that |token_id| is well-formed. Converts |token_id| into the output
// parameter |keystore|. Only populated on success. Returns an empty string on
// success and an error message on error. A validation error should result in
// extension termination.
std::string ValidateInput(const std::string& token_id,
                          crosapi::mojom::KeystoreType* keystore) {
  base::Optional<crosapi::mojom::KeystoreType> keystore_type =
      KeystoreTypeFromString(token_id);
  if (!keystore_type)
    return kInvalidKeystoreType;
  *keystore = keystore_type.value();

  return "";
}

// Similar to ValidateInput() above but also converts |signing_algorithm| into
// the output parameter |out_signing|. Only populated on success. A validation
// error should result in extension termination.
std::string ValidateInput(
    const std::string& token_id,
    crosapi::mojom::KeystoreType* keystore,
    const api_epki::Algorithm& signing_algorithm,
    crosapi::mojom::KeystoreSigningAlgorithmPtr* out_signing) {
  std::string error = ValidateInput(token_id, keystore);
  if (!error.empty())
    return error;

  *out_signing = crosapi::mojom::KeystoreSigningAlgorithm::New();
  if (signing_algorithm.name == "RSASSA-PKCS1-v1_5") {
    if (!signing_algorithm.modulus_length ||
        *signing_algorithm.modulus_length < 0) {
      return kUnsupportedSigningInput;
    }
    crosapi::mojom::KeystorePKCS115ParamsPtr params =
        crosapi::mojom::KeystorePKCS115Params::New();
    if (!base::IsValueInRangeForNumericType<uint32_t>(
            *(signing_algorithm.modulus_length))) {
      return kUnsupportedSigningInput;
    }
    params->modulus_length =
        static_cast<uint32_t>(*(signing_algorithm.modulus_length));
    (*out_signing)->set_pkcs115(std::move(params));
    return "";
  } else if (signing_algorithm.name == "ECDSA") {
    if (!signing_algorithm.named_curve) {
      return kUnsupportedSigningInput;
    }
    crosapi::mojom::KeystoreECDSAParamsPtr params =
        crosapi::mojom::KeystoreECDSAParams::New();
    params->named_curve = *(signing_algorithm.named_curve);
    (*out_signing)->set_ecdsa(std::move(params));
    return "";
  }

  return kUnsupportedSigning;
}

}  // namespace

ExtensionFunction::ResponseAction LacrosNotImplementedExtensionFunction::Run() {
  return RespondNow(Error(kLacrosNotImplementedError));
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGenerateKeyFunction::Run() {
  std::unique_ptr<api_epki::GenerateKey::Params> params(
      api_epki::GenerateKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(KeystoreService::kGenerateKeyMinVersion,
                                      browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  crosapi::mojom::KeystoreSigningAlgorithmPtr signing;
  error =
      ValidateInput(params->token_id, &keystore, params->algorithm, &signing);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGenerateKey, this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->GenerateKey(keystore, std::move(signing), extension_id(), std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGenerateKey(
    ResultPtr result) {
  using Result = crosapi::mojom::KeystoreBinaryResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::BLOB:
      Respond(ArgumentList(
          api_epki::GenerateKey::Results::Create(result->get_blob())));
      return;
  }
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysGetCertificatesFunction::Run() {
  std::unique_ptr<api_epk::GetCertificates::Params> params(
      api_epk::GetCertificates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(
      KeystoreService::kGetCertificatesMinVersion, browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysGetCertificatesFunction::OnGetCertificates, this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->GetCertificates(keystore, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysGetCertificatesFunction::OnGetCertificates(
    ResultPtr result) {
  using Result = crosapi::mojom::GetCertificatesResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::CERTIFICATES:
      auto client_certs = std::make_unique<base::ListValue>();
      for (std::vector<uint8_t>& cert : result->get_certificates()) {
        client_certs->Append(std::make_unique<base::Value>(std::move(cert)));
      }

      auto results = std::make_unique<base::ListValue>();
      results->Append(std::move(client_certs));
      Respond(ArgumentList(std::move(results)));
      return;
  }
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysImportCertificateFunction::Run() {
  std::unique_ptr<api_epk::ImportCertificate::Params> params(
      api_epk::ImportCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(
      KeystoreService::kAddCertificateMinVersion, browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysImportCertificateFunction::OnAddCertificate, this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->AddCertificate(keystore, params->certificate, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysImportCertificateFunction::OnAddCertificate(
    const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysRemoveCertificateFunction::Run() {
  std::unique_ptr<api_epk::RemoveCertificate::Params> params(
      api_epk::RemoveCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(
      KeystoreService::kRemoveCertificateMinVersion, browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  crosapi::mojom::KeystoreType keystore;
  error = ValidateInput(params->token_id, &keystore);
  EXTENSION_FUNCTION_VALIDATE(error.empty());

  auto c = base::BindOnce(
      &EnterprisePlatformKeysRemoveCertificateFunction::OnRemoveCertificate,
      this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->RemoveCertificate(keystore, params->certificate, std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysRemoveCertificateFunction::OnRemoveCertificate(
    const std::string& error) {
  if (error.empty()) {
    Respond(NoArguments());
  } else {
    Respond(Error(error));
  }
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGetTokensFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args_->empty());

  std::string error = ValidateCrosapi(KeystoreService::kGetKeyStoresMinVersion,
                                      browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  auto c = base::BindOnce(
      &EnterprisePlatformKeysInternalGetTokensFunction::OnGetKeyStores, this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->GetKeyStores(std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGetTokensFunction::OnGetKeyStores(
    ResultPtr result) {
  using Result = crosapi::mojom::GetKeyStoresResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::KEY_STORES:
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
      return;
  }
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeMachineKey::Params> params(
      api_epk::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string error = ValidateCrosapi(
      KeystoreService::kChallengeAttestationOnlyKeystoreMinVersion,
      browser_context());
  if (!error.empty())
    return RespondNow(Error(error));

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeMachineKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->ChallengeAttestationOnlyKeystore(
          StringFromVector(params->challenge),
          crosapi::mojom::KeystoreType::kDevice,
          /*migrate=*/params->register_key ? *params->register_key : false,
          std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::
    OnChallengeAttestationOnlyKeystore(ResultPtr result) {
  using Result = crosapi::mojom::KeystoreStringResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::CHALLENGE_RESPONSE:
      Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
          VectorFromString(result->get_challenge_response()))));
      return;
  }
}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeUserKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeUserKey::Params> params(
      api_epk::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const std::string error = ValidateCrosapi(
      KeystoreService::kChallengeAttestationOnlyKeystoreMinVersion,
      browser_context());
  if (!error.empty())
    return RespondNow(Error(error));

  if (!platform_keys::IsExtensionAllowed(
          Profile::FromBrowserContext(browser_context()), extension())) {
    return RespondNow(Error(kExtensionDoesNotHavePermission));
  }

  auto c = base::BindOnce(&EnterprisePlatformKeysChallengeUserKeyFunction::
                              OnChallengeAttestationOnlyKeystore,
                          this);
  chromeos::LacrosChromeServiceImpl::Get()
      ->keystore_service_remote()
      ->ChallengeAttestationOnlyKeystore(StringFromVector(params->challenge),
                                         crosapi::mojom::KeystoreType::kUser,
                                         /*migrate=*/params->register_key,
                                         std::move(c));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::
    OnChallengeAttestationOnlyKeystore(ResultPtr result) {
  using Result = crosapi::mojom::KeystoreStringResult;
  switch (result->which()) {
    case Result::Tag::ERROR_MESSAGE:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::CHALLENGE_RESPONSE:
      Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
          VectorFromString(result->get_challenge_response()))));
      return;
  }
}

}  // namespace extensions
