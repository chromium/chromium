// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/testing_rlwe_id_provider.h"

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy::psm {

TestingRlweIdProvider::TestingRlweIdProvider(
    const private_membership::rlwe::RlwePlaintextId& testing_psm_device_rlwe_id)
    : testing_psm_device_rlwe_id_(testing_psm_device_rlwe_id) {}

psm_rlwe::RlwePlaintextId TestingRlweIdProvider::ConstructRlweId() {
  return testing_psm_device_rlwe_id_;
}

}  // namespace policy::psm
