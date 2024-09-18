// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "chromeos/crosapi/mojom/keystore_error.mojom-shared.h"
#include "chromeos/crosapi/mojom/keystore_service.mojom.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "net/base/net_errors.h"
#include "net/cert/asn1_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_util.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/keystore_service_ash.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#endif  // #if BUILDFLAG(IS_CHROMEOS_ASH)

using PublicKeyInfo = chromeos::platform_keys::PublicKeyInfo;

namespace extensions {

namespace {

namespace api_pk = api::platform_keys;
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
const char kErrorInteractiveCallFromBackground[] =
    "Interactive calls must happen in the context of a browser tab or a "
    "window.";
const char kErrorInvalidSpki[] = "The SubjectPublicKeyInfo is not valid.";

// Skip checking for interactive calls coming from a non-interactive
// context.
// TODO(crbug.com/40217298): We should move the interactive tests to a
// separate test suite. This is a temporary workaround to allow these
// tests to run from the test extension's background page.
bool g_skip_interactive_check_for_test = false;

const struct NameValuePair {
  const char* const name;
  const int value;
} kCertStatusErrors[] = {
#define CERT_STATUS_FLAG(name, value) {#name, value},
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG
};

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
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service || !service->IsAvailable<crosapi::mojom::KeystoreService>())
    return kUnsupportedByAsh;

  int version = service->GetInterfaceVersion<KeystoreService>();
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

std::optional<SigningAlgorithmName> SigningAlgorithmNameFromString(
    const std::string& input) {
  if (input == kWebCryptoRsassaPkcs1v15)
    return SigningAlgorithmName::kRsassaPkcs115;
  if (input == kWebCryptoEcdsa)
    return SigningAlgorithmName::kEcdsa;
  return std::nullopt;
}

}  // namespace

//------------------------------------------------------------------------------
PlatformKeysInternalSelectClientCertificatesFunction::
    ~PlatformKeysInternalSelectClientCertificatesFunction() {}

void PlatformKeysInternalSelectClientCertificatesFunction::
    SetSkipInteractiveCheckForTest(bool skip_interactive_check) {
  g_skip_interactive_check_for_test = skip_interactive_check;
}

ExtensionFunction::ResponseAction
PlatformKeysInternalSelectClientCertificatesFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::optional<api_pki::SelectClientCertificates::Params> params =
      api_pki::SelectClientCertificates::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::platform_keys::ClientCertificateRequest request;
  request.certificate_authorities =
      std::move(params->details.request.certificate_authorities);

  for (const api_pk::ClientCertificateType& cert_type :
       params->details.request.certificate_types) {
    switch (cert_type) {
      case api_pk::ClientCertificateType::kEcdsaSign:
        request.certificate_key_types.push_back(
            net::X509Certificate::kPublicKeyTypeECDSA);
        break;
      case api_pk::ClientCertificateType::kRsaSign:
        request.certificate_key_types.push_back(
            net::X509Certificate::kPublicKeyTypeRSA);
        break;
      case api_pk::ClientCertificateType::kNone:
        NOTREACHED_IN_MIGRATION();
    }
  }

  std::unique_ptr<net::CertificateList> client_certs;
  if (params->details.client_certs) {
    client_certs = std::make_unique<net::CertificateList>();
    for (const std::vector<uint8_t>& client_cert_der :
         *params->details.client_certs) {
      if (client_cert_der.empty())
        return RespondNow(Error(platform_keys::kErrorInvalidX509Cert));
      // Allow UTF-8 inside PrintableStrings in client certificates. See
      // crbug.com/770323 and crbug.com/788655.
      net::X509Certificate::UnsafeCreateOptions options;
      options.printable_string_is_utf8 = true;
      scoped_refptr<net::X509Certificate> client_cert_x509 =
          net::X509Certificate::CreateFromBytesUnsafeOptions(client_cert_der,
                                                             options);
      if (!client_cert_x509)
        return RespondNow(Error(platform_keys::kErrorInvalidX509Cert));
      client_certs->push_back(client_cert_x509);
    }
  }

  content::WebContents* web_contents = nullptr;
  if (params->details.interactive) {
    web_contents = GetSenderWebContents();

    // Ensure that this function is called in a context that allows opening
    // dialogs.
    if ((!web_contents ||
         !web_modal::WebContentsModalDialogManager::FromWebContents(
             web_contents)) &&
        !g_skip_interactive_check_for_test) {
      return RespondNow(Error(kErrorInteractiveCallFromBackground));
    }
  }

  chromeos::ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  service->SelectClientCertificates(
      request, std::move(client_certs), params->details.interactive,
      extension_id(),
      base::BindOnce(&PlatformKeysInternalSelectClientCertificatesFunction::
                         OnSelectedCertificates,
                     this),
      web_contents);
  return RespondLater();
}

void PlatformKeysInternalSelectClientCertificatesFunction::
    OnSelectedCertificates(std::unique_ptr<net::CertificateList> matches,
                           std::optional<crosapi::mojom::KeystoreError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (error) {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(error.value())));
    return;
  }

  DCHECK(matches);
  std::vector<api_pk::Match> result_matches;
  for (const scoped_refptr<net::X509Certificate>& match : *matches) {
    PublicKeyInfo key_info;
    key_info.public_key_spki_der =
        chromeos::platform_keys::GetSubjectPublicKeyInfo(match);
    if (!chromeos::platform_keys::GetPublicKey(match, &key_info.key_type,
                                               &key_info.key_size_bits)) {
      LOG(ERROR) << "Could not retrieve public key info.";
      continue;
    }

    api_pk::Match result_match;
    std::string_view der_encoded_cert =
        net::x509_util::CryptoBufferAsStringPiece(match->cert_buffer());
    result_match.certificate.assign(der_encoded_cert.begin(),
                                    der_encoded_cert.end());

    std::optional<base::Value::Dict> algorithm =
        BuildWebCryptoAlgorithmDictionary(key_info);
    if (!algorithm) {
      LOG(ERROR) << "Skipping unsupported certificate with key type "
                 << key_info.key_type;
      continue;
    }
    result_match.key_algorithm.additional_properties = std::move(*algorithm);

    result_matches.push_back(std::move(result_match));
  }
  Respond(ArgumentList(
      api_pki::SelectClientCertificates::Results::Create(result_matches)));
}

//------------------------------------------------------------------------------

PlatformKeysInternalGetPublicKeyFunction::
    ~PlatformKeysInternalGetPublicKeyFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyFunction::Run() {
  std::optional<api_pki::GetPublicKey::Params> params =
      api_pki::GetPublicKey::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::string error = ValidateCrosapi(KeystoreService::kGetPublicKeyMinVersion,
                                      browser_context());
  if (!error.empty()) {
    return RespondNow(Error(error));
  }

  std::optional<SigningAlgorithmName> algorithm_name =
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
  if (result->is_error()) {
    Respond(Error(
        chromeos::platform_keys::KeystoreErrorToString(result->get_error())));
    return;
  }

  api_pki::GetPublicKey::Results::Algorithm algorithm;
  std::optional<base::Value::Dict> dict =
      crosapi::keystore_service_util::DictionaryFromSigningAlgorithm(
          result->get_success_result()->algorithm_properties);
  if (!dict) {
    Respond(Error(kErrorInvalidSigningAlgorithm));
    return;
  }
  algorithm.additional_properties = std::move(*dict);
  Respond(ArgumentList(api_pki::GetPublicKey::Results::Create(
      result->get_success_result()->public_key, std::move(algorithm))));
}
//------------------------------------------------------------------------------

PlatformKeysInternalGetPublicKeyBySpkiFunction::
    ~PlatformKeysInternalGetPublicKeyBySpkiFunction() = default;

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyBySpkiFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(b/191958380): Lift the restriction when *.platformKeys.* APIs are
  // implemented for secondary profiles in Lacros.
  if (!Profile::FromBrowserContext(browser_context())->IsMainProfile())
    return RespondNow(Error(kUnsupportedProfile));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  std::optional<api_pki::GetPublicKeyBySpki::Params> params =
      api_pki::GetPublicKeyBySpki::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const auto& public_key_spki_der = params->public_key_spki_der;
  if (public_key_spki_der.empty())
    return RespondNow(Error(kErrorInvalidSpki));

  PublicKeyInfo key_info;
  key_info.public_key_spki_der.assign(std::begin(public_key_spki_der),
                                      std::end(public_key_spki_der));

  if (!chromeos::platform_keys::GetPublicKeyBySpki(key_info.public_key_spki_der,
                                                   &key_info.key_type,
                                                   &key_info.key_size_bits)) {
    return RespondNow(Error(StatusToString(
        chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
  }

  chromeos::platform_keys::Status check_result =
      chromeos::platform_keys::CheckKeyTypeAndAlgorithm(key_info.key_type,
                                                        params->algorithm_name);
  if (check_result != chromeos::platform_keys::Status::kSuccess)
    return RespondNow(Error(StatusToString(check_result)));

  api_pki::GetPublicKeyBySpki::Results::Algorithm algorithm;
  std::optional<base::Value::Dict> algorithm_dictionary =
      chromeos::platform_keys::BuildWebCryptoAlgorithmDictionary(key_info);
  DCHECK(algorithm_dictionary);
  algorithm.additional_properties = std::move(*algorithm_dictionary);

  return RespondNow(ArgumentList(api_pki::GetPublicKeyBySpki::Results::Create(
      public_key_spki_der, algorithm)));
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

  std::optional<api_pki::Sign::Params> params =
      api_pki::Sign::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<chromeos::platform_keys::TokenId> platform_keys_token_id;
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
        platform_keys_token_id, std::move(params->data),
        std::move(params->public_key), extension_id(),
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
        platform_keys_token_id, std::move(params->data),
        std::move(params->public_key), key_type, hash_algorithm, extension_id(),
        base::BindOnce(&PlatformKeysInternalSignFunction::OnSigned, this));
  }

  return RespondLater();
}

void PlatformKeysInternalSignFunction::OnSigned(
    std::vector<uint8_t> signature,
    std::optional<crosapi::mojom::KeystoreError> error) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!error) {
    Respond(ArgumentList(api_pki::Sign::Results::Create(std::move(signature))));
  } else {
    Respond(
        Error(chromeos::platform_keys::KeystoreErrorToString(error.value())));
  }
}

//------------------------------------------------------------------------------

PlatformKeysVerifyTLSServerCertificateFunction::
    ~PlatformKeysVerifyTLSServerCertificateFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysVerifyTLSServerCertificateFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::optional<api_pk::VerifyTLSServerCertificate::Params> params =
      api_pk::VerifyTLSServerCertificate::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  VerifyTrustAPI::GetFactoryInstance()
      ->Get(browser_context())
      ->Verify(std::move(params), extension_id(),
               base::BindOnce(&PlatformKeysVerifyTLSServerCertificateFunction::
                                  FinishedVerification,
                              this));

  return RespondLater();
}

void PlatformKeysVerifyTLSServerCertificateFunction::FinishedVerification(
    const std::string& error,
    int verify_result,
    int cert_status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!error.empty()) {
    Respond(Error(error));
    return;
  }

  api_pk::VerificationResult result;
  result.trusted = verify_result == net::OK;
  if (net::IsCertificateError(verify_result)) {
    // Only report errors, not internal informational statuses.
    const int masked_cert_status = cert_status & net::CERT_STATUS_ALL_ERRORS;
    for (auto status_error : kCertStatusErrors) {
      if ((masked_cert_status & status_error.value) == status_error.value) {
        result.debug_errors.push_back(status_error.name);
      }
    }
  }

  Respond(ArgumentList(
      api_pk::VerifyTLSServerCertificate::Results::Create(result)));
}

}  // namespace extensions
