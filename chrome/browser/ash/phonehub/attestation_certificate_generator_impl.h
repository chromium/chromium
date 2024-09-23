// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PHONEHUB_ATTESTATION_CERTIFICATE_GENERATOR_IMPL_H_
#define CHROME_BROWSER_ASH_PHONEHUB_ATTESTATION_CERTIFICATE_GENERATOR_IMPL_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/soft_bind_attestation_flow.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/phonehub/public/cpp/attestation_certificate_generator.h"
#include "chromeos/ash/services/device_sync/cryptauth_key_registry.h"

namespace ash::phonehub {

class AttestationCertificateGeneratorImpl
    : public AttestationCertificateGenerator,
      public NetworkStateHandlerObserver {
 public:
  AttestationCertificateGeneratorImpl(
      Profile* profile,
      std::unique_ptr<attestation::SoftBindAttestationFlow>
          soft_bind_attestation_flow);

  ~AttestationCertificateGeneratorImpl() override;

  // AttestationCertificateGenerator:
  void RetrieveCertificate() override;

  // NetworkStateHandlerObserver:
  void DefaultNetworkChanged(const NetworkState* network) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(AttestationCertificateGeneratorImplTest,
                           RegenerateAfterExpiration);
  FRIEND_TEST_ALL_PREFIXES(AttestationCertificateGeneratorImplTest,
                           RetrieveCertificateWithoutCache);

  bool ShouldRegenerateAttestationCertificate();
  void GenerateCertificate();
  void OnAttestationCertificateGenerated(
      const std::vector<std::string>& attestation_certs,
      bool is_valid);

  std::unique_ptr<attestation::SoftBindAttestationFlow>
      soft_bind_attestation_flow_;
  std::unique_ptr<device_sync::CryptAuthKeyRegistry> key_registry_;
  raw_ptr<Profile, DanglingUntriaged> profile_;
  bool is_valid_ = false;
  std::vector<std::string> attestation_certs_;
  base::Time last_attestation_completed_time_;
  base::Time last_attestation_attempt_from_network_change_time_;
  base::WeakPtrFactory<AttestationCertificateGeneratorImpl> weak_ptr_factory_{
      this};
};
}  // namespace ash::phonehub

#endif  // CHROME_BROWSER_ASH_PHONEHUB_ATTESTATION_CERTIFICATE_GENERATOR_IMPL_H_
