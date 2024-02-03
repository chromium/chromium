// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/common.h"

#include "base/logging.h"
#include "base/rand_util.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"
#include "crypto/encryptor.h"
#include "crypto/hkdf.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"

bool IsNearbyShareCertificateExpired(base::Time current_time,
                                     base::Time not_after,
                                     bool use_public_certificate_tolerance) {
  base::TimeDelta tolerance =
      use_public_certificate_tolerance
          ? kNearbySharePublicCertificateValidityBoundOffsetTolerance
          : base::Seconds(0);

  return current_time >= not_after + tolerance;
}

bool IsNearbyShareCertificateWithinValidityPeriod(
    base::Time current_time,
    base::Time not_before,
    base::Time not_after,
    bool use_public_certificate_tolerance) {
  base::TimeDelta tolerance =
      use_public_certificate_tolerance
          ? kNearbySharePublicCertificateValidityBoundOffsetTolerance
          : base::Seconds(0);

  return current_time >= not_before - tolerance &&
         !IsNearbyShareCertificateExpired(current_time, not_after,
                                          use_public_certificate_tolerance);
}

std::vector<uint8_t> DeriveNearbyShareKey(base::span<const uint8_t> key,
                                          size_t new_num_bytes) {
  return crypto::HkdfSha256(key,
                            /*salt=*/base::span<const uint8_t>(),
                            /*info=*/base::span<const uint8_t>(),
                            new_num_bytes);
}

std::vector<uint8_t> ComputeAuthenticationTokenHash(
    base::span<const uint8_t> authentication_token,
    base::span<const uint8_t> secret_key) {
  return crypto::HkdfSha256(authentication_token, secret_key,
                            /*info=*/base::span<const uint8_t>(),
                            kNearbyShareNumBytesAuthenticationTokenHash);
}

std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes) {
  return crypto::RandBytesAsVector(num_bytes);
}

std::unique_ptr<crypto::Encryptor> CreateNearbyShareCtrEncryptor(
    const crypto::SymmetricKey* secret_key,
    base::span<const uint8_t> salt) {
  DCHECK(secret_key);
  DCHECK_EQ(kNearbyShareNumBytesSecretKey, secret_key->key().size());
  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt, salt.size());

  std::unique_ptr<crypto::Encryptor> encryptor =
      std::make_unique<crypto::Encryptor>();

  // For CTR mode, the iv input to Init() must be empty. Instead, the iv is
  // set via SetCounter().
  if (!encryptor->Init(secret_key, crypto::Encryptor::Mode::CTR,
                       /*iv=*/base::span<const uint8_t>())) {
    LOG(ERROR) << "Encryptor could not be initialized.";
    return nullptr;
  }

  std::vector<uint8_t> iv =
      DeriveNearbyShareKey(salt, kNearbyShareNumBytesAesCtrIv);
  if (!encryptor->SetCounter(iv)) {
    LOG(ERROR) << "Could not set encryptor counter.";
    return nullptr;
  }

  return encryptor;
}
