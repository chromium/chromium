// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"

#include "device_management_backend.pb.h"
#include "private_membership_rlwe.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace policy {

class RlweTestSupportTest
    : public testing::Test,
      public testing::WithParamInterface</*is_member*/ bool> {};

// Test that the RlweClient created via CreateClientFactory matches the
// RlweTestCase loaded vid LoadTestCase.
TEST_P(RlweTestSupportTest, CaseMatchesFactory) {
  const bool is_member = GetParam();
  const auto test_case = psm::testing::LoadTestCase(is_member);
  auto rlwe_client = psm::testing::CreateClientFactory(is_member).Run(
      private_membership::rlwe::CROS_DEVICE_STATE,
      private_membership::rlwe::RlwePlaintextId());

  // Verify that the expected membership matches the test parameter.
  EXPECT_EQ(test_case.is_positive_membership_expected(), is_member);

  // Create and compare OPRF request.
  const auto status_or_orpf = rlwe_client->CreateOprfRequest();
  ASSERT_TRUE(status_or_orpf.ok());
  EXPECT_EQ(status_or_orpf.value().SerializeAsString(),
            test_case.expected_oprf_request().SerializeAsString());

  // Create and compare Query request.
  // This also test that the OPRF response meets the RlweClient's expectations.
  const auto status_or_query =
      rlwe_client->CreateQueryRequest(test_case.oprf_response());
  ASSERT_TRUE(status_or_query.ok());
  EXPECT_EQ(status_or_query.value().SerializeAsString(),
            test_case.expected_query_request().SerializeAsString());
}

INSTANTIATE_TEST_SUITE_P(RlweTestSupportTestSuite,
                         RlweTestSupportTest,
                         /* is_member*/ testing::Bool());

}  // namespace policy
