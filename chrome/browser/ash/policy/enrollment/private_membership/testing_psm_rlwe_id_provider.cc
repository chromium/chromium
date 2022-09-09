// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/private_membership/testing_psm_rlwe_id_provider.h"

#include "third_party/private_membership/src/private_membership_rlwe.pb.h"

namespace psm_rlwe = private_membership::rlwe;

namespace policy {

TestingPsmRlweIdProvider::TestingPsmRlweIdProvider(
    const private_membership::rlwe::RlwePlaintextId& testing_psm_device_rlwe_id)
    : testing_psm_device_rlwe_id_(testing_psm_device_rlwe_id) {}

psm_rlwe::RlwePlaintextId TestingPsmRlweIdProvider::ConstructRlweId() {
  return testing_psm_device_rlwe_id_;
}

}  // namespace policy
