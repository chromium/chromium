// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_CONSTRUCT_RLWE_ID_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_CONSTRUCT_RLWE_ID_H_

namespace private_membership::rlwe {
class RlwePlaintextId;
}  // namespace private_membership::rlwe

namespace policy::psm {

// Constructs the encrypted PSM RLWE ID through device's serial number
// and RLZ brand code that will be retrieved through StatisticsProvider.
// For more information, see go/psm-rlwe-id.
//
// Note: The device's serial number and RLZ brand code values must exist and
// able to be retrieved, using their corresponding keys, from the
// StaisticsProvider. Otherwise the implementation will CHECK-fail.
private_membership::rlwe::RlwePlaintextId ConstructRlweId();

}  // namespace policy::psm

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_CONSTRUCT_RLWE_ID_H_
