// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PHONEHUB_ATTESTATION_CERTIFICATE_GENERATOR_IMPL_H_
#define CHROME_BROWSER_ASH_PHONEHUB_ATTESTATION_CERTIFICATE_GENERATOR_IMPL_H_

#include "chrome/browser/ash/attestation/soft_bind_attestation_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"

namespace ash::phonehub {

class AttestationCertificateGeneratorImpl
    : public AttestationCertificateGenerator {
 public:
  AttestationCertificateGeneratorImpl(
      Profile* profile,
      std::unique_ptr<attestation::SoftBindAttestationFlow>
          soft_bind_attestation_flow);

  ~AttestationCertificateGeneratorImpl() override;

  // AttestationCertificateGenerator:
  void GenerateCertificate(OnCertificateGeneratedCallback callback) override;

 private:
  std::unique_ptr<attestation::SoftBindAttestationFlow>
      soft_bind_attestation_flow_;
  std::unique_ptr<device_sync::CryptAuthKeyRegistry> key_registry_;
  Profile* profile_;
};
}  // namespace ash::phonehub

#endif  // CHROME_BROWSER_ASH_PHONEHUB_ATTESTATION_CERTIFICATE_GENERATOR_IMPL_H_
