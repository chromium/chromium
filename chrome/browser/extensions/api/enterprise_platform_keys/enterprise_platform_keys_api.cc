// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_platform_keys/enterprise_platform_keys_api.h"

#include <utility>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/values.h"
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
  // TODO(pneubeck): Add support for unsigned integers to IDL.
  EXTENSION_FUNCTION_VALIDATE(params && params->modulus_length >= 0);
  std::string platform_keys_token_id;
  if (!platform_keys::ValidateToken(params->token_id, &platform_keys_token_id))
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  chromeos::PlatformKeysService* service =
      chromeos::PlatformKeysServiceFactory::GetForBrowserContext(
          browser_context());
  DCHECK(service);

  service->GenerateRSAKey(
      platform_keys_token_id, params->modulus_length, extension_id(),
      base::Bind(
          &EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey,
          this));
  return RespondLater();
}

void EnterprisePlatformKeysInternalGenerateKeyFunction::OnGeneratedKey(
    const std::string& public_key_der,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error_message.empty()) {
    Respond(ArgumentList(api_epki::GenerateKey::Results::Create(
        std::vector<uint8_t>(public_key_der.begin(), public_key_der.end()))));
  } else {
    Respond(Error(error_message));
  }
}

EnterprisePlatformKeysGetCertificatesFunction::
    ~EnterprisePlatformKeysGetCertificatesFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysGetCertificatesFunction::Run() {
  std::unique_ptr<api_epk::GetCertificates::Params> params(
      api_epk::GetCertificates::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string platform_keys_token_id;
  if (!platform_keys::ValidateToken(params->token_id, &platform_keys_token_id))
    return RespondNow(Error(platform_keys::kErrorInvalidToken));

  chromeos::platform_keys::GetCertificates(
      platform_keys_token_id,
      base::Bind(
          &EnterprisePlatformKeysGetCertificatesFunction::OnGotCertificates,
          this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysGetCertificatesFunction::OnGotCertificates(
    std::unique_ptr<net::CertificateList> certs,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error_message.empty()) {
    Respond(Error(error_message));
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
  std::string platform_keys_token_id;
  if (!platform_keys::ValidateToken(params->token_id, &platform_keys_token_id))
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

  chromeos::platform_keys::ImportCertificate(
      platform_keys_token_id, cert_x509,
      base::Bind(&EnterprisePlatformKeysImportCertificateFunction::
                     OnImportedCertificate,
                 this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysImportCertificateFunction::OnImportedCertificate(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error_message.empty())
    Respond(NoArguments());
  else
    Respond(Error(error_message));
}

EnterprisePlatformKeysRemoveCertificateFunction::
    ~EnterprisePlatformKeysRemoveCertificateFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysRemoveCertificateFunction::Run() {
  std::unique_ptr<api_epk::RemoveCertificate::Params> params(
      api_epk::RemoveCertificate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  std::string platform_keys_token_id;
  if (!platform_keys::ValidateToken(params->token_id, &platform_keys_token_id))
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

  chromeos::platform_keys::RemoveCertificate(
      platform_keys_token_id, cert_x509,
      base::Bind(&EnterprisePlatformKeysRemoveCertificateFunction::
                     OnRemovedCertificate,
                 this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysRemoveCertificateFunction::OnRemovedCertificate(
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (error_message.empty())
    Respond(NoArguments());
  else
    Respond(Error(error_message));
}

EnterprisePlatformKeysInternalGetTokensFunction::
    ~EnterprisePlatformKeysInternalGetTokensFunction() {}

ExtensionFunction::ResponseAction
EnterprisePlatformKeysInternalGetTokensFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args_->empty());

  chromeos::platform_keys::GetTokens(
      base::Bind(&EnterprisePlatformKeysInternalGetTokensFunction::OnGotTokens,
                 this),
      browser_context());
  return RespondLater();
}

void EnterprisePlatformKeysInternalGetTokensFunction::OnGotTokens(
    std::unique_ptr<std::vector<std::string>> platform_keys_token_ids,
    const std::string& error_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error_message.empty()) {
    Respond(Error(error_message));
    return;
  }

  std::vector<std::string> token_ids;
  for (std::vector<std::string>::const_iterator it =
           platform_keys_token_ids->begin();
       it != platform_keys_token_ids->end(); ++it) {
    std::string token_id = platform_keys::PlatformKeysTokenIdToApiId(*it);
    if (token_id.empty()) {
      Respond(Error(kEnterprisePlatformErrorInternal));
      return;
    }
    token_ids.push_back(token_id);
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
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeMachineKeyFunction::OnChallengedKey(
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epk::ChallengeMachineKey::Results::Create(
        VectorFromString(result.data))));
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
  chromeos::attestation::TpmChallengeKeyCallback callback = base::Bind(
      &EnterprisePlatformKeysChallengeUserKeyFunction::OnChallengedKey, this);
  // base::Unretained is safe on impl_ since its life-cycle matches |this| and
  // |callback| holds a reference to |this|.
  base::OnceClosure task = base::BindOnce(
      &EPKPChallengeKey::Run, base::Unretained(&impl_),
      chromeos::attestation::KEY_USER, scoped_refptr<ExtensionFunction>(this),
      std::move(callback), StringFromVector(params->challenge),
      params->register_key);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, std::move(task));
  return RespondLater();
}

void EnterprisePlatformKeysChallengeUserKeyFunction::OnChallengedKey(
    const chromeos::attestation::TpmChallengeKeyResult& result) {
  if (result.IsSuccess()) {
    Respond(ArgumentList(api_epk::ChallengeUserKey::Results::Create(
        VectorFromString(result.data))));
  } else {
    Respond(Error(result.GetErrorMessage()));
  }
}

}  // namespace extensions
