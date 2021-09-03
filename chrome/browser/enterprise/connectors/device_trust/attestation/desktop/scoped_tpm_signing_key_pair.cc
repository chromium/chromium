// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/scoped_tpm_signing_key_pair.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace enterprise_connectors {
namespace test {

ScopedTpmSigningKeyPair::ScopedTpmSigningKeyPair() {
  // Generating and forcing the usage of a mocked TPM wrapped key to allow
  // tests to skip having to rotate/store the key itself.
  auto provider = crypto::GetUnexportableKeyProvider();
  DCHECK(provider);
  const crypto::SignatureVerifier::SignatureAlgorithm algorithms[] = {
      crypto::SignatureVerifier::ECDSA_SHA256};
  auto signing_key = provider->GenerateSigningKeySlowly(algorithms);
  DCHECK(signing_key);
  wrapped_key_ = signing_key->GetWrappedKey();
  SigningKeyPair::SetTpmKeyWrappedForTesting(wrapped_key_);
}

ScopedTpmSigningKeyPair::~ScopedTpmSigningKeyPair() {
  SigningKeyPair::ClearTpmKeyWrappedForTesting();
}

}  // namespace test
}  // namespace enterprise_connectors
