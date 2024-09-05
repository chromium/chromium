// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/kcer/kcer_utils.h"

#include "ash/components/kcer/kcer.h"

namespace kcer {

std::vector<SigningScheme> GetSupportedSigningSchemes(bool supports_pss,
                                                      KeyType key_type) {
  std::vector<SigningScheme> result;

  switch (key_type) {
      // Supported signing schemes for RSA also depend on the key length, but
      // NSS doesn't seem to provide a convenient interface to read it. 2048 bit
      // keys are big enough for all RSA signatures, smaller keys are not really
      // used in practice nowadays and the TLS stack is expected to also double
      // check and shrink the list.
    case KeyType::kRsa:
      result.insert(result.end(), {
                                      SigningScheme::kRsaPkcs1Sha1,
                                      SigningScheme::kRsaPkcs1Sha256,
                                      SigningScheme::kRsaPkcs1Sha384,
                                      SigningScheme::kRsaPkcs1Sha512,
                                  });
      if (supports_pss) {
        result.insert(result.end(), {
                                        SigningScheme::kRsaPssRsaeSha256,
                                        SigningScheme::kRsaPssRsaeSha384,
                                        SigningScheme::kRsaPssRsaeSha512,
                                    });
      }
      break;
    case KeyType::kEcc:
      result.insert(result.end(), {
                                      SigningScheme::kEcdsaSecp256r1Sha256,
                                      SigningScheme::kEcdsaSecp384r1Sha384,
                                      SigningScheme::kEcdsaSecp521r1Sha512,
                                  });
  }

  return result;
}

}  // namespace kcer
