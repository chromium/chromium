// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_ID_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_ID_PROVIDER_H_

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_id_provider.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace policy::psm {

// PSM RLWE device ID for testing purposes which is returning the passed RLWE ID
// without encryption.
class TestingRlweIdProvider : public RlweIdProvider {
 public:
  explicit TestingRlweIdProvider(
      const private_membership::rlwe::RlwePlaintextId&
          testing_psm_device_rlwe_id);

  // `TestingRlweIdProvider` is neither copyable nor copy assignable.
  TestingRlweIdProvider(const TestingRlweIdProvider&) = delete;
  TestingRlweIdProvider& operator=(const TestingRlweIdProvider&) = delete;

  ~TestingRlweIdProvider() override = default;

  // Returns `testing_psm_device_rlwe_id_` directly for testing purposes.
  private_membership::rlwe::RlwePlaintextId ConstructRlweId() override;

 private:
  // PSM RLWE device ID used for testing purposes.
  const private_membership::rlwe::RlwePlaintextId testing_psm_device_rlwe_id_;
};

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_TESTING_RLWE_ID_PROVIDER_H_
