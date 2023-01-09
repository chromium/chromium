// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ATTESTATION_FAKE_SOFT_BIND_ATTESTATION_FLOW_H_
#define CHROME_BROWSER_ASH_ATTESTATION_FAKE_SOFT_BIND_ATTESTATION_FLOW_H_

#include "chrome/browser/ash/attestation/soft_bind_attestation_flow.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash::attestation {

class FakeSoftBindAttestationFlow : public SoftBindAttestationFlow {
 public:
  FakeSoftBindAttestationFlow();

  FakeSoftBindAttestationFlow(const FakeSoftBindAttestationFlow&) = delete;
  FakeSoftBindAttestationFlow& operator=(const FakeSoftBindAttestationFlow&) =
      delete;

  ~FakeSoftBindAttestationFlow() override;

  void SetCertificates(std::vector<std::string> certs);

  // SoftBindAttestationFlow:
  void GetCertificate(Callback callback,
                      const AccountId& account_id,
                      const std::string& user_key) override;

 private:
  std::vector<std::string> certs_;
};

}  // namespace ash::attestation

#endif  // CHROME_BROWSER_ASH_ATTESTATION_FAKE_SOFT_BIND_ATTESTATION_FLOW_H_
