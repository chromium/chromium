// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_KCER_SSL_PRIVATE_KEY_KCER_H_
#define ASH_COMPONENTS_KCER_SSL_PRIVATE_KEY_KCER_H_

#include <stdint.h>

#include <string>

#include "ash/components/kcer/kcer.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "net/ssl/ssl_private_key.h"

namespace kcer {

// Exported for unit tests only.
class COMPONENT_EXPORT(KCER) SSLPrivateKeyKcer : public net::SSLPrivateKey {
 public:
  SSLPrivateKeyKcer(base::WeakPtr<Kcer> kcer,
                    scoped_refptr<const Cert> cert,
                    KeyType key_type,
                    base::flat_set<SigningScheme> supported_schemes);

  // Implements SSLPrivateKey.
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override;

 private:
  ~SSLPrivateKeyKcer() override;

  void OnSigned(SignCallback callback, base::expected<Signature, Error> result);

  // The Kcer instance that should be used to work with `cert_`.
  base::WeakPtr<Kcer> kcer_;
  // The certificate that is related to the private key, which will be used for
  // Sign(). Used as a handler for the key.
  scoped_refptr<const Cert> cert_;
  // Supported signing algorithms for the key (as OpenSSL SSL_* constants),
  // sorted by preference.
  std::vector<uint16_t> algorithm_preferences_;
  base::WeakPtrFactory<SSLPrivateKeyKcer> weak_factory_{this};
};

}  // namespace kcer

#endif  // ASH_COMPONENTS_KCER_SSL_PRIVATE_KEY_KCER_H_
