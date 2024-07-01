// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_SSL_PRIVATE_KEY_KCER_H_
#define CHROME_BROWSER_ASH_NET_SSL_PRIVATE_KEY_KCER_H_

// TMP QCERT
#include "base/memory/weak_ptr.h"
#include "chromeos/components/kcer/kcer.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

class NET_EXPORT SSLPrivateKeyKcer : public SSLPrivateKey {
 public:
  SSLPrivateKeyKcer(base::WeakPtr<kcer::Kcer> kcer,
                    scoped_refptr<const kcer::Cert> cert,
                    int evp_key_type,
                    base::flat_set<kcer::SigningScheme> supported_schemes);

  // Implements SSLPrivateKey.
  std::string GetProviderName() override;
  std::vector<uint16_t> GetAlgorithmPreferences() override;
  void Sign(uint16_t algorithm,
            base::span<const uint8_t> input,
            SignCallback callback) override;

 private:
  ~SSLPrivateKeyKcer() override;

  void OnSigned(SignCallback callback,
                base::expected<kcer::Signature, kcer::Error> result);

  base::WeakPtr<kcer::Kcer> kcer_;
  scoped_refptr<const kcer::Cert> cert_;
  std::vector<uint16_t> algorithm_preferences_;
  base::WeakPtrFactory<SSLPrivateKeyKcer> weak_factory_{this};
};

}  // namespace net

#endif  // CHROME_BROWSER_ASH_NET_SSL_PRIVATE_KEY_KCER_H_
