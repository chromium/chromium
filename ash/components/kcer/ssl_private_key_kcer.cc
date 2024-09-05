// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/ssl_private_key_kcer.h"

#include <prerror.h>
#include <stdint.h>

#include <string>

#include "ash/components/kcer/kcer.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/stringprintf.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace kcer {
namespace {
void LogError(const char* message, Error kcer_error) {
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
}

int KcerKeyTypeToEvp(KeyType key_type) {
  switch (key_type) {
    case KeyType::kRsa:
      return EVP_PKEY_RSA;
    case KeyType::kEcc:
      return EVP_PKEY_EC;
  }
}

}  // namespace

SSLPrivateKeyKcer::SSLPrivateKeyKcer(
    base::WeakPtr<Kcer> kcer,
    scoped_refptr<const Cert> cert,
    KeyType key_type,
    base::flat_set<SigningScheme> supported_schemes)
    : kcer_(std::move(kcer)), cert_(std::move(cert)) {
  // For the first step always assume that everything is supported (i.e. PSS),
  // that will be filtered based on the actual supported schemes.
  algorithm_preferences_ = DefaultAlgorithmPreferences(
      KcerKeyTypeToEvp(key_type), /*supports_pss=*/true);
  std::erase_if(algorithm_preferences_, [&](uint16_t pref) {
    return !supported_schemes.contains(static_cast<SigningScheme>(pref));
  });
}

SSLPrivateKeyKcer::~SSLPrivateKeyKcer() = default;

std::string SSLPrivateKeyKcer::GetProviderName() {
  switch (cert_->GetToken()) {
    case Token::kUser:
      return "chaps:UserToken";
    case Token::kDevice:
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
    return std::move(callback).Run(net::Error::ERR_CONTEXT_SHUT_DOWN,
                                   std::vector<uint8_t>());
  }
  if (!cert_) {
    return std::move(callback).Run(net::Error::ERR_UNEXPECTED,
                                   std::vector<uint8_t>());
  }

  kcer_->Sign(PrivateKeyHandle(*cert_), static_cast<SigningScheme>(algorithm),
              DataToSign(std::vector<uint8_t>(input.begin(), input.end())),
              base::BindOnce(&SSLPrivateKeyKcer::OnSigned,
                             weak_factory_.GetWeakPtr(), std::move(callback)));
}

void SSLPrivateKeyKcer::OnSigned(SignCallback callback,
                                 base::expected<Signature, Error> result) {
  if (!result.has_value()) {
    LogError("Sign failed", result.error());
    return std::move(callback).Run(
        net::Error::ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED,
        std::vector<uint8_t>());
  }

  return std::move(callback).Run(net::Error::OK, result->value());
}

}  // namespace kcer
