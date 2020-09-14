// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api_ash.h"

#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/extension_platform_keys_service_factory.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service.h"
#include "chrome/browser/chromeos/platform_keys/platform_keys_service_factory.h"
#include "chrome/browser/extensions/api/platform_keys/platform_keys_api.h"
#include "chrome/common/extensions/api/enterprise_platform_keys.h"
#include "chrome/common/extensions/api/enterprise_platform_keys_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace extensions {

namespace {

namespace api_epk = api::enterprise_platform_keys;
namespace api_epki = api::enterprise_platform_keys_internal;

// This error will occur if a token is removed and will be exposed to the
// extension. Keep this in sync with the custom binding in Javascript.
const char kEnterprisePlatformErrorInternal[] = "Internal Error.";

const char kEnterprisePlatformErrorInvalidX509Cert[] =
    "Certificate is not a valid X.509 certificate.";

std::vector<uint8_t> VectorFromString(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::string StringFromVector(const std::vector<uint8_t>& v) {
  return std::string(v.begin(), v.end());
}

}  // namespace

EnterprisePlatformKeysInternalGenerateKeyFunction::
    ~EnterprisePlatformKeysInternalGenerateKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGenerateKeyFunction::Run() {
  std::unique_ptr<api_epki::GenerateKey::Params> params(
      api_epki::GenerateKey::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params);
  base::Optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id)
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

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
        extension_id(),
        base::Bind(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else if (params->algorithm.name == "ECDSA") {
    EXTENSION_FUNCTION_VALIDATE(params->algorithm.named_curve);
    service->GenerateECKey(
        platform_keys_token_id.value(), *(params->algorithm.named_curve),
        extension_id(),
        base::Bind(
            &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
            this));
  } else {
    NOTREACHED();
    EXTENSION_FUNCTION_VALIDATE(false);
  }
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    const std::string& public_key_der,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess) {
    Respond(ArgumentList(api_epki::GenerateKey::Results::Create(
        std::vector<uint8_t>(public_key_der.begin(), public_key_der.end()))));
  } else {
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
  }
}

EnterprisePlatformKeysGetCertificatesFunction::
    ~EnterprisePlatformKeysGetCertificatesFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysGetCertificatesFunction::Run() {
  std::unique_ptr<api_epk::GetCertificates::Params> params(
      api_epk::GetCertificates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id)
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  chromeos::platform_keys::PlatformKeysService* platform_keys_service =
      chromeos::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  platform_keys_service->GetCertificates(
      platform_keys_token_id.value(),
      base::Bind(
          &EnterprisePlatformKeysGetCertificatesFunction::OnGotCertificates,
          this));
  return RespondLater();
}

void EnterprisePlatformKeysGetCertificatesFunction::OnGotCertificates(
    std::unique_ptr<net::CertificateList> certs,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != chromeos::platform_keys::Status::kSuccess) {
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
    return;
  }

  std::unique_ptr<base::ListValue> client_certs(new base::ListValue());
  for (net::CertificateList::const_iterator it = certs->begin();
       it != certs->end(); ++it) {
    base::StringPiece cert_der =
        net::x509_util::CryptoBufferAsStringPiece((*it)->cert_buffer());
    client_certs->Append(std::make_unique<base::Value>(
        base::Value::BlobStorage(cert_der.begin(), cert_der.end())));
  }

  std::unique_ptr<base::ListValue> results(new base::ListValue());
  results->Append(std::move(client_certs));
  Respond(ArgumentList(std::move(results)));
}

EnterprisePlatformKeysImportCertificateFunction::
    ~EnterprisePlatformKeysImportCertificateFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysImportCertificateFunction::Run() {
  std::unique_ptr<api_epk::ImportCertificate::Params> params(
      api_epk::ImportCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id)
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  const std::vector<uint8_t>& cert_der = params->certificate;
  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<net::X509Certificate> cert_x509 =
      net::X509Certificate::CreateFromBytesUnsafeOptions(
          reinterpret_cast<const char*>(cert_der.data()), cert_der.size(),
          options);
  if (!cert_x509.get())
    return RespondNow(Error(kEnterprisePlatformErrorInvalidX509Cert));

  chromeos::platform_keys::PlatformKeysService* platform_keys_service =
      chromeos::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  CHECK(platform_keys_service);

  platform_keys_service->ImportCertificate(
      platform_keys_token_id.value(), cert_x509,
      base::Bind(&EnterprisePlatformKeysImportCertificateFunction::
                     OnImportedCertificate,
                 this));
  return RespondLater();
}

void EnterprisePlatformKeysImportCertificateFunction::OnImportedCertificate(
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess)
    Respond(NoArguments());
  else
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
}

EnterprisePlatformKeysRemoveCertificateFunction::
    ~EnterprisePlatformKeysRemoveCertificateFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysRemoveCertificateFunction::Run() {
  std::unique_ptr<api_epk::RemoveCertificate::Params> params(
      api_epk::RemoveCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  base::Optional<chromeos::platform_keys::TokenId> platform_keys_token_id =
      platform_keys::ApiIdToPlatformKeysTokenId(params->token_id);
  if (!platform_keys_token_id)
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  const std::vector<uint8_t>& cert_der = params->certificate;
  // Allow UTF-8 inside PrintableStrings in client certificates. See
  // crbug.com/770323 and crbug.com/788655.
  net::X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<net::X509Certificate> cert_x509 =
      net::X509Certificate::CreateFromBytesUnsafeOptions(
          reinterpret_cast<const char*>(cert_der.data()), cert_der.size(),
          options);
  if (!cert_x509.get())
    return RespondNow(Error(kEnterprisePlatformErrorInvalidX509Cert));

  chromeos::platform_keys::PlatformKeysService* platform_keys_service =
      chromeos::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  CHECK(platform_keys_service);

  platform_keys_service->RemoveCertificate(
      platform_keys_token_id.value(), cert_x509,
      base::Bind(&EnterprisePlatformKeysRemoveCertificateFunction::
                     OnRemovedCertificate,
                 this));
  return RespondLater();
}

void EnterprisePlatformKeysRemoveCertificateFunction::OnRemovedCertificate(
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status == chromeos::platform_keys::Status::kSuccess)
    Respond(NoArguments());
  else
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
}

EnterprisePlatformKeysInternalGetTokensFunction::
    ~EnterprisePlatformKeysInternalGetTokensFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGetTokensFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args_->empty());

  chromeos::platform_keys::PlatformKeysService* platform_keys_service =
      chromeos::platform_keys::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  CHECK(platform_keys_service);

  platform_keys_service->GetTokens(base::BindOnce(
      &EnterprisePlatformKeysInternalGetTokensFunction::OnGotTokens, this));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGetTokensFunction::OnGotTokens(
    std::unique_ptr<std::vector<chromeos::platform_keys::TokenId>>
        platform_keys_token_ids,
    chromeos::platform_keys::Status status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (status != chromeos::platform_keys::Status::kSuccess) {
    Respond(Error(chromeos::platform_keys::StatusToString(status)));
    return;
  }

  std::vector<std::string> token_ids;
  for (auto token_id : *platform_keys_token_ids) {
    std::string api_token_id =
        platform_keys::PlatformKeysTokenIdToApiId(token_id);
    if (api_token_id.empty()) {
      Respond(Error(kEnterprisePlatformErrorInternal));
      return;
    }
    token_ids.push_back(api_token_id);
  }

  Respond(ArgumentList(api_epki::GetTokens::Results::Create(token_ids)));
}

EnterprisePlatformKeysChallengeMachineKeyFunction::
    EnterprisePlatformKeysChallengeMachineKeyFunction() = default;

EnterprisePlatformKeysChallengeMachineKeyFunction::
    ~EnterprisePlatformKeysChallengeMachineKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeMachineKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeMachineKey::Params> params(
      api_epk::ChallengeMachineKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  chromeos::attestation::TpmChallengeKeyCallback callback = base::BindOnce(
      &EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKey,
      this);
  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_DEVICE, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), StringFromVector(params->challenge),
      params->register_key ? *params->register_key : false);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKey(
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
        VectorFromString(result.challenge_response))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

EnterprisePlatformKeysChallengeUserKeyFunction::
    EnterprisePlatformKeysChallengeUserKeyFunction() = default;

EnterprisePlatformKeysChallengeUserKeyFunction::
    ~EnterprisePlatformKeysChallengeUserKeyFunction() = default;

ExtensionFunction::ResponseAction
EnterprisePlatformKeysChallengeUserKeyFunction::Run() {
  std::unique_ptr<api_epk::ChallengeUserKey::Params> params(
      api_epk::ChallengeUserKey::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  chromeos::attestation::TpmChallengeKeyCallback callback = base::BindOnce(
      &EnterprisePlatformKeysChallengeUserKeyFunction::OnChallengedKey, this);
  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_USER, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), StringFromVector(params->challenge),
      params->register_key);
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::OnChallengedKey(
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
        VectorFromString(result.challenge_response))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

}  // namespace extensions
