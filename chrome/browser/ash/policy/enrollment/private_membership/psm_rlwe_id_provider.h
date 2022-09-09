// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_ID_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_ID_PROVIDER_H_

namespace private_membership {
namespace rlwe {
class RlwePlaintextId;
}  // namespace rlwe
}  // namespace private_membership

namespace policy {

// Interface for the PSM RLWE device ID, allowing to discard the
// PSM ID encryption for tests.
class PsmRlweIdProvider {
 public:
  virtual ~PsmRlweIdProvider() = default;

  virtual private_membership::rlwe::RlwePlaintextId ConstructRlweId() = 0;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_PSM_RLWE_ID_PROVIDER_H_
