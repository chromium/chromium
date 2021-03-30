// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/platform_keys_api_ash.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"
#include "chrome/common/extensions/api/platform_keys_internal.h"
#include "chromeos/crosapi/cpp/keystore_service_util.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

using PublicKeyInfo = chromeos::platform_keys::PublicKeyInfo;

namespace extensions {

namespace api_pk = api::platform_keys;
namespace api_pki = api::platform_keys_internal;

namespace {

const char kErrorInteractiveCallFromBackground[] =
    "Interactive calls must happen in the context of a browser tab or a "
    "window.";

using crosapi::keystore_service_util::kWebCryptoEcdsa;
using crosapi::keystore_service_util::kWebCryptoRsassaPkcs1v15;

const struct NameValuePair {
  const char* const name;
  const int value;
} kCertStatusErrors[] = {
#define CERT_STATUS_FLAG(name, value) {#name, value},
#include "net/cert/cert_status_flags_list.h"
#undef CERT_STATUS_FLAG
};

}  // namespace

namespace platform_keys {

const char kErrorInvalidSpki[] = "The SubjectPublicKeyInfo is not valid.";
const char kErrorInvalidToken[] = "The token is not valid.";
const char kErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";
const char kTokenIdUser[] = "user";
const char kTokenIdSystem[] = "system";

base::Optional<chromeos::platform_keys::TokenId> ApiIdToPlatformKeysTokenId(
    const std::string& token_id) {
  if (token_id == kTokenIdUser)
    return chromeos::platform_keys::TokenId::kUser;

  if (token_id == kTokenIdSystem)
    return chromeos::platform_keys::TokenId::kSystem;

  return base::nullopt;
}

std::string PlatformKeysTokenIdToApiId(
    chromeos::platform_keys::TokenId platform_keys_token_id) {
  switch (platform_keys_token_id) {
    case chromeos::platform_keys::TokenId::kUser:
      return kTokenIdUser;
    case chromeos::platform_keys::TokenId::kSystem:
      return kTokenIdSystem;
  }
}

}  // namespace platform_keys

PlatformKeysInternalGetPublicKeyFunction::
    ~PlatformKeysInternalGetPublicKeyFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyFunction::Run() {
  std::unique_ptr<api_pki::GetPublicKey::Params> params(
      api_pki::GetPublicKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::platform_keys::GetPublicKeyAndAlgorithmOutput output =
      chromeos::platform_keys::GetPublicKeyAndAlgorithm(params->certificate,
                                                        params->algorithm_name);
  if (output.status != chromeos::platform_keys::Status::kSuccess) {
    return RespondNow(
        Error(chromeos::platform_keys::StatusToString(output.status)));
  }

  api_pki::GetPublicKey::Results::Algorithm algorithm;
  algorithm.additional_properties = std::move(output.algorithm);
  return RespondNow(ArgumentList(api_pki::GetPublicKey::Results::Create(
      std::move(output.public_key), std::move(algorithm))));
}

PlatformKeysInternalGetPublicKeyBySpkiFunction::
    ~PlatformKeysInternalGetPublicKeyBySpkiFunction() = default;

ExtensionFunction::ResponseAction
PlatformKeysInternalGetPublicKeyBySpkiFunction::Run() {
  std::unique_ptr<api_pki::GetPublicKeyBySpki::Params> params(
      api_pki::GetPublicKeyBySpki::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const auto& public_key_spki_der = params->public_key_spki_der;
  if (public_key_spki_der.empty())
    return RespondNow(Error(platform_keys::kErrorInvalidSpki));

  PublicKeyInfo key_info;
  key_info.public_key_spki_der.assign(std::begin(public_key_spki_der),
                                      std::end(public_key_spki_der));

  if (!chromeos::platform_keys::GetPublicKeyBySpki(key_info.public_key_spki_der,
                                                   &key_info.key_type,
                                                   &key_info.key_size_bits) ||
      (key_info.key_type != net::X509Certificate::kPublicKeyTypeRSA &&
       key_info.key_type != net::X509Certificate::kPublicKeyTypeECDSA)) {
    return RespondNow(Error(StatusToString(
        chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
  }

  // Currently, the only supported combinations are either an SPKI declaring
  // rsaEncryption used with the RSASSA-PKCS1-v1.5 algorithm or an SPKI
  // declaring id-ecPublicKey used with P-256 curve name.
  api_pki::GetPublicKeyBySpki::Results::Algorithm algorithm;
  if (params->algorithm_name == kWebCryptoRsassaPkcs1v15) {
    chromeos::platform_keys::BuildWebCryptoRSAAlgorithmDictionary(
        key_info, &algorithm.additional_properties);
  } else if (params->algorithm_name == kWebCryptoEcdsa) {
    chromeos::platform_keys::BuildWebCryptoEcdsaAlgorithmDictionary(
        key_info, &algorithm.additional_properties);
  } else {
    return RespondNow(Error(StatusToString(
        chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));
  }

  return RespondNow(ArgumentList(api_pki::GetPublicKeyBySpki::Results::Create(
      public_key_spki_der, algorithm)));
}

PlatformKeysInternalSelectClientCertificatesFunction::
    ~PlatformKeysInternalSelectClientCertificatesFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysInternalSelectClientCertificatesFunction::Run() {
  std::unique_ptr<api_pki::SelectClientCertificates::Params> params(
      api_pki::SelectClientCertificates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  chromeos::ExtensionPlatformKeysService* service =
      chromeos::ExtensionPlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  chromeos::platform_keys::ClientCertificateRequest request;
  for (const std::vector<uint8_t>& cert_authority :
       params->details.request.certificate_authorities) {
    request.certificate_authorities.push_back(
        std::string(cert_authority.begin(), cert_authority.end()));
  }
  for (const api_pk::ClientCertificateType& cert_type :
       params->details.request.certificate_types) {
    switch (cert_type) {
      case api_pk::CLIENT_CERTIFICATE_TYPE_ECDSASIGN:
        request.certificate_key_types.push_back(
            net::X509Certificate::kPublicKeyTypeECDSA);
        break;
      case api_pk::CLIENT_CERTIFICATE_TYPE_RSASIGN:
        request.certificate_key_types.push_back(
            net::X509Certificate::kPublicKeyTypeRSA);
        break;
      case api_pk::CLIENT_CERTIFICATE_TYPE_NONE:
        NOTREACHED();
    }
  }

  std::unique_ptr<net::CertificateList> client_certs;
  if (params->details.client_certs) {
    client_certs.reset(new net::CertificateList);
    for (const std::vector<uint8_t>& client_cert_der :
         *params->details.client_certs) {
      if (client_cert_der.empty())
        return RespondNow(Error(platform_keys::kErrorInvalidX509Cert));
      // Allow UTF-8 inside PrintableStrings in client certificates. See
      // crbug.com/770323 and crbug.com/788655.
      net::X509Certificate::UnsafeCreateOptions options;
      options.printable_string_is_utf8 = true;
      scoped_refptr<net::X509Certificate> client_cert_x509 =
          net::X509Certificate::CreateFromBytesUnsafeOptions(
              reinterpret_cast<const char*>(client_cert_der.data()),
              client_cert_der.size(), options);
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
    if (!web_contents ||
        !web_modal::WebContentsModalDialogManager::FromWebContents(
            web_contents)) {
      return RespondNow(Error(kErrorInteractiveCallFromBackground));
    }
  }

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
                           chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != chromeos::platform_keys::Status::kSuccess) {
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
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
    if (key_info.key_type != net::X509Certificate::kPublicKeyTypeRSA) {
      LOG(ERROR) << "Skipping unsupported certificate with non-RSA key.";
      continue;
    }

    api_pk::Match result_match;
    base::StringPiece der_encoded_cert =
        net::x509_util::CryptoBufferAsStringPiece(match->cert_buffer());
    result_match.certificate.assign(der_encoded_cert.begin(),
                                    der_encoded_cert.end());

    chromeos::platform_keys::BuildWebCryptoRSAAlgorithmDictionary(
        key_info, &result_match.key_algorithm.additional_properties);
    result_matches.push_back(std::move(result_match));
  }
  Respond(ArgumentList(
      api_pki::SelectClientCertificates::Results::Create(result_matches)));
}

PlatformKeysInternalSignFunction::~PlatformKeysInternalSignFunction() {}

ExtensionFunction::ResponseAction PlatformKeysInternalSignFunction::Run() {
  std::unique_ptr<api_pki::Sign::Params> params(
      api_pki::Sign::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  base::Optional<chromeos::platform_keys::TokenId> platform_keys_token_id;
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
    if (params->algorithm_name != kWebCryptoRsassaPkcs1v15)
      return RespondNow(Error(StatusToString(
          chromeos::platform_keys::Status::kErrorAlgorithmNotSupported)));

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
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status == chromeos::platform_keys::Status::kSuccess)
    Respond(ArgumentList(api_pki::Sign::Results::Create(
        std::vector<uint8_t>(signature.begin(), signature.end()))));
  else
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
}

PlatformKeysVerifyTLSServerCertificateFunction::
    ~PlatformKeysVerifyTLSServerCertificateFunction() {}

ExtensionFunction::ResponseAction
PlatformKeysVerifyTLSServerCertificateFunction::Run() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<api_pk::VerifyTLSServerCertificate::Params> params(
      api_pk::VerifyTLSServerCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

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
    for (size_t i = 0; i < base::size(kCertStatusErrors); ++i) {
      if ((masked_cert_status & kCertStatusErrors[i].value) ==
          kCertStatusErrors[i].value) {
        result.debug_errors.push_back(kCertStatusErrors[i].name);
      }
    }
  }

  Respond(ArgumentList(
      api_pk::VerifyTLSServerCertificate::Results::Create(result)));
}

}  // namespace extensions
