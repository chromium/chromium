// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/request_handler_for_psm_auto_enrollment.h"
#include "private_membership_rlwe.pb.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace em = enterprise_management;
using RlweClient = private_membership::rlwe::PrivateMembershipRlweClient;

namespace policy::psm::testing {

namespace {

std::unique_ptr<RlweClient> CreateRlweClient(
    const RlweTestCase& test_case,
    private_membership::rlwe::RlweUseCase /* unused */,
    const private_membership::rlwe::RlwePlaintextId& /* unused*/) {
  auto status_or_client =
      private_membership::rlwe::PrivateMembershipRlweClient::CreateForTesting(
          private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE,
          {test_case.plaintext_id()}, test_case.ec_cipher_key(),
          test_case.seed());  // IN-TEST
  CHECK(status_or_client.ok()) << status_or_client.status().message();

  return std::move(status_or_client).value();
}

}  // namespace

RlweTestCase LoadTestCase(bool is_member) {
  const auto test_data = RequestHandlerForPsmAutoEnrollment::LoadTestData();
  DCHECK(test_data);

  for (const auto& test_case : test_data->test_cases()) {
    if (test_case.is_positive_membership_expected() == is_member) {
      return test_case;
    }
  }

  CHECK(false) << "Could not find psm test data for is_member == " << is_member;
  return {};
}

RlweClientFactory CreateClientFactory(bool is_member) {
  return base::BindRepeating(&CreateRlweClient, LoadTestCase(is_member));
}

}  // namespace policy::psm::testing
