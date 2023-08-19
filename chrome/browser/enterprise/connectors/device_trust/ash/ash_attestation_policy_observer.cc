// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/ash/ash_attestation_policy_observer.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/ash/ash_attestation_service.h"

namespace enterprise_connectors {

AshAttestationPolicyObserver::AshAttestationPolicyObserver(
    base::WeakPtr<AshAttestationService> attestation_service)
    : attestation_service_(std::move(attestation_service)) {
  CHECK(attestation_service_);
}

AshAttestationPolicyObserver::~AshAttestationPolicyObserver() = default;

void AshAttestationPolicyObserver::OnInlinePolicyEnabled(DTCPolicyLevel level) {
  if (!attestation_service_) {
    return;
  }

  attestation_service_->TryPrepareKey();
}
}  // namespace enterprise_connectors
