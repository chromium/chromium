// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/attestation/fake_soft_bind_attestation_flow.h"

namespace ash::attestation {

FakeSoftBindAttestationFlow::FakeSoftBindAttestationFlow() = default;

FakeSoftBindAttestationFlow::~FakeSoftBindAttestationFlow() = default;

void FakeSoftBindAttestationFlow::GetCertificate(
    FakeSoftBindAttestationFlow::Callback callback,
    const AccountId& account_id,
    const std::string& user_key) {
  if (std::size(certs_) == 0) {
    std::move(callback).Run({}, false);
  } else {
    std::move(callback).Run(certs_, true);
  }
}

void FakeSoftBindAttestationFlow::SetCertificates(
    std::vector<std::string> certs) {
  certs_ = certs;
}

}  // namespace ash::attestation
