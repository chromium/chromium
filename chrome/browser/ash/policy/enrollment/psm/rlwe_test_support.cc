// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/psm/rlwe_test_support.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/policy/enrollment/psm/rlwe_client_impl.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "third_party/private_membership/src/internal/testing/regression_test_data/regression_test_data.pb.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"

namespace em = enterprise_management;
using RlweTestData =
    private_membership::rlwe::PrivateMembershipRlweClientRegressionTestData;

namespace policy::psm::testing {

namespace {

std::unique_ptr<RlweClient> CreateRlweClient(
    const RlweTestCase& test_case,
    const private_membership::rlwe::RlwePlaintextId& /* unused*/) {
  auto status_or_client =
      private_membership::rlwe::PrivateMembershipRlweClient::CreateForTesting(
          private_membership::rlwe::RlweUseCase::CROS_DEVICE_STATE,
          {test_case.plaintext_id()}, test_case.ec_cipher_key(),
          test_case.seed());  // IN-TEST
  CHECK(status_or_client.ok()) << status_or_client.status().message();

  return std::make_unique<RlweClientImpl>(std::move(status_or_client).value(),
                                          test_case.plaintext_id());
}

RlweTestData ReadTestData() {
  base::FilePath src_root_dir;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &src_root_dir));
  const base::FilePath path_to_test_data =
      src_root_dir.AppendASCII("third_party")
          .AppendASCII("private_membership")
          .AppendASCII("src")
          .AppendASCII("internal")
          .AppendASCII("testing")
          .AppendASCII("regression_test_data")
          .AppendASCII("test_data.binarypb");

  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(base::PathExists(path_to_test_data))
      << " path_to_test_data: " << path_to_test_data;

  std::string serialized_test_data;
  CHECK(base::ReadFileToString(path_to_test_data, &serialized_test_data));

  RlweTestData test_data;
  CHECK(test_data.ParseFromString(serialized_test_data));

  return test_data;
}

}  // namespace

RlweTestCase LoadTestCase(bool is_member) {
  const auto test_data = ReadTestData();

  for (const auto& test_case : test_data.test_cases()) {
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
