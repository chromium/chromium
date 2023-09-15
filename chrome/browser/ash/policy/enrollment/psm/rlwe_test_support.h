// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_TEST_SUPPORT_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_TEST_SUPPORT_H_

#include <memory>

#include "base/functional/callback.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"

namespace private_membership::rlwe {
class RlwePlaintextId;
class PrivateMembershipRlweClient;
}  // namespace private_membership::rlwe

namespace policy::psm::testing {

using RlweTestCase = private_membership::rlwe::
    PrivateMembershipRlweClientRegressionTestData::TestCase;

using RlweClientFactory = base::RepeatingCallback<
    std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>(
        private_membership::rlwe::RlweUseCase,
        const private_membership::rlwe::RlwePlaintextId&)>;

// Load a test case from the test database provided by the
// third_party/private_membership library, i.e. a test case that is also used in
// the EmbeddedPolicyTestServer. This test case can be used to simulate PSM
// requests for members or non-members of the set.
//
// Note that this function performs a blocking file read.
RlweTestCase LoadTestCase(bool is_member);

// Load a test case from the test database provided by the
// third_party/private_membership library. Based on that test case,
// create a callback as needed by the `AutoEnrollmentController` to create an
// `RlweClient`. The returned callback will ignore the plaintext id and
// instead create an `RlweClient` that will create requests as expected by the
// EmbeddedPolicyTestServer.
RlweClientFactory CreateClientFactory(bool is_member = true);

}  // namespace policy::psm::testing

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_PSM_RLWE_TEST_SUPPORT_H_
