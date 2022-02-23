// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_ID_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_ID_PROVIDER_IMPL_H_

#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider.h"

namespace private_membership {
namespace rlwe {
class RlwePlaintextId;
}  // namespace rlwe
}  // namespace private_membership

namespace policy {

// Real implementation for the PSM RLWE device ID which is encrypting the PSM
// ID.
class PsmRlweIdProviderImpl : public PsmRlweIdProvider {
 public:
  PsmRlweIdProviderImpl() = default;

  // `PsmRlweIdProviderImpl` is neither copyable nor copy assignable.
  PsmRlweIdProviderImpl(const PsmRlweIdProviderImpl&) = delete;
  PsmRlweIdProviderImpl& operator=(const PsmRlweIdProviderImpl&) = delete;

  ~PsmRlweIdProviderImpl() override = default;

  // Constructs the encrypted PSM RLWE ID through device's serial number
  // and RLZ brand code that will be retrieved through StatisticsProvider.
  // For more information, see go/psm-rlwe-id.
  //
  // Note: The device's serial number and RLZ brand code values must exist and
  // able to be retrieved, using their corresponding keys, from the
  // StaisticsProvider. Otherwise the implementation will CHECK-fail.
  private_membership::rlwe::RlwePlaintextId ConstructRlweId() override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_ID_PROVIDER_IMPL_H_
