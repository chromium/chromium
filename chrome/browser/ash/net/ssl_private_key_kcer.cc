// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/ssl_private_key_kcer.h"

#include <prerror.h>

// TMP QCERT
#include "net/base/net_errors.h"

// TMP QCERT
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

// TMP QCERT
#include "base/base64.h"
#include "base/debug/stack_trace.h"
#include "base/debug/task_trace.h"
#include "base/logging.h"
#define HERE() LOG(ERROR) << " === QCERT " << __FUNCTION__ << " "
// #define HERE() LAZY_STREAM(LOG_STREAM(ERROR), /*is_on=*/false)

namespace net {
namespace {
void LogError(const char* message, kcer::Error kcer_error) {
  // TODO(miersh): When/if NSS is fully removed, this will be removed as well.
  // For now could still be useful.
  PRErrorCode err = PR_GetError();
  const char* err_name = PR_ErrorToName(err);
  if (err_name == nullptr) {
    err_name = "";
  }

  LOG(ERROR) << base::StringPrintf("%s, kcer_error: %d, nss_error: %d (%s)",
                                   message, static_cast<int>(kcer_error), err,
                                   err_name);
  HERE() << "FAIL "
         << base::StringPrintf("%s, kcer_error: %d, nss_error: %d (%s)",
                               message, static_cast<int>(kcer_error), err,
                               err_name);
}

}  // namespace

SSLPrivateKeyKcer::SSLPrivateKeyKcer(
    base::WeakPtr<kcer::Kcer> kcer,
    scoped_refptr<const kcer::Cert> cert,
    int evp_key_type,
    base::flat_set<kcer::SigningScheme> supported_schemes)
    : kcer_(std::move(kcer)), cert_(std::move(cert)) {
  // For the first step always assume that everything is supported (i.e. PSS),
  // that will be filtered based on the actual supported schemes.
  algorithm_preferences_ =
      DefaultAlgorithmPreferences(evp_key_type, /*supports_pss=*/true);
  std::erase_if(algorithm_preferences_, [&](uint16_t pref) {
    return !supported_schemes.contains(static_cast<kcer::SigningScheme>(pref));
  });
}

SSLPrivateKeyKcer::~SSLPrivateKeyKcer() = default;

std::string SSLPrivateKeyKcer::GetProviderName() {
  switch (cert_->GetToken()) {
    case kcer::Token::kUser:
      return "chaps:UserToken";
    case kcer::Token::kDevice:
      return "chaps:DeviceToken";
  }
}

std::vector<uint16_t> SSLPrivateKeyKcer::GetAlgorithmPreferences() {
  return algorithm_preferences_;
}

void SSLPrivateKeyKcer::Sign(uint16_t algorithm,
                             base::span<const uint8_t> input,
                             SignCallback callback) {
  if (!kcer_) {
    HERE() << "FAIL this is not expected to happen";
    return std::move(callback).Run(net::Error::ERR_CONTEXT_SHUT_DOWN,
                                   std::vector<uint8_t>());
  }

  HERE() << "ok " << base::Base64Encode(cert_->GetPkcs11Id().value());
  kcer_->Sign(
      kcer::PrivateKeyHandle(*cert_),
      static_cast<kcer::SigningScheme>(algorithm),
      kcer::DataToSign(std::vector<uint8_t>(input.begin(), input.end())),
      base::BindOnce(&SSLPrivateKeyKcer::OnSigned, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void SSLPrivateKeyKcer::OnSigned(
    SignCallback callback,
    base::expected<kcer::Signature, kcer::Error> result) {
  if (!result.has_value()) {
    HERE() << "FAIL SIGN FAILED";
    LogError("Sign failed", result.error());
    return std::move(callback).Run(
        net::Error::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
        std::vector<uint8_t>());
  }

  HERE() << "ok SIGN SUCCESS " << result.value()->size();
  return std::move(callback).Run(net::Error::OK, result->value());
}

}  // namespace net
