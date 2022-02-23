// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_TESTING_PSM_RLWE_ID_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_TESTING_PSM_RLWE_ID_PROVIDER_H_

#include "chrome/browser/ash/policy/enrollment/private_membership/psm_rlwe_id_provider.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace policy {

// PSM RLWE device ID for testing purposes which is returning the passed RLWE ID
// without encryption.
class TestingPsmRlweIdProvider : public PsmRlweIdProvider {
 public:
  explicit TestingPsmRlweIdProvider(
      const private_membership::rlwe::RlwePlaintextId&
          testing_psm_device_rlwe_id);

  // `TestingPsmRlweIdProvider` is neither copyable nor copy assignable.
  TestingPsmRlweIdProvider(const TestingPsmRlweIdProvider&) = delete;
  TestingPsmRlweIdProvider& operator=(const TestingPsmRlweIdProvider&) = delete;

  ~TestingPsmRlweIdProvider() override = default;

  // Returns `testing_psm_device_rlwe_id_` directly for testing purposes.
  private_membership::rlwe::RlwePlaintextId ConstructRlweId() override;

 private:
  // PSM RLWE device ID used for testing purposes.
  const private_membership::rlwe::RlwePlaintextId testing_psm_device_rlwe_id_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PRIVATE_MEMBERSHIP_TESTING_PSM_RLWE_ID_PROVIDER_H_
