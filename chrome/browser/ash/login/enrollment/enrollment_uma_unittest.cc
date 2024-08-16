// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_uma.h"

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "components/policy/core/common/cloud/enterprise_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

struct TokenBasedEnrollmentOOBEConfigUMATestCase {
  policy::OOBEConfigSource oobe_config_source;
  std::string expected_oobe_config_source_variant;
};

TokenBasedEnrollmentOOBEConfigUMATestCase test_cases[] = {
    {policy::OOBEConfigSource::kNone, "None"},
    {policy::OOBEConfigSource::kUnknown, "Unknown"},
    {policy::OOBEConfigSource::kRemoteDeployment, "RemoteDeployment"},
    {policy::OOBEConfigSource::kPackagingTool, "PackagingTool"}};

class TokenBasedEnrollmentUMASuccessTest
    : public testing::TestWithParam<TokenBasedEnrollmentOOBEConfigUMATestCase> {
};

TEST_P(TokenBasedEnrollmentUMASuccessTest, Success) {
  base::HistogramTester histogram_tester;
  TokenBasedEnrollmentOOBEConfigUMATestCase test_case = GetParam();

  ash::TokenBasedEnrollmentOOBEConfigUMA(
      policy::EnrollmentStatus::ForEnrollmentCode(
          policy::EnrollmentStatus::Code::kSuccess),
      test_case.oobe_config_source);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Enterprise.TokenBasedEnrollmentOobeConfig.",
                    test_case.expected_oobe_config_source_variant, ".Success"}),
      true, 1);
}

class TokenBasedEnrollmentUMAFailureTest
    : public testing::TestWithParam<TokenBasedEnrollmentOOBEConfigUMATestCase> {
};

TEST_P(TokenBasedEnrollmentUMAFailureTest, Failure) {
  base::HistogramTester histogram_tester;
  TokenBasedEnrollmentOOBEConfigUMATestCase test_case = GetParam();

  ash::TokenBasedEnrollmentOOBEConfigUMA(
      policy::EnrollmentStatus::ForEnrollmentCode(
          policy::EnrollmentStatus::Code::kRegistrationFailed),
      test_case.oobe_config_source);

  histogram_tester.ExpectUniqueSample(
      base::StrCat({"Enterprise.TokenBasedEnrollmentOobeConfig.",
                    test_case.expected_oobe_config_source_variant, ".Success"}),
      false, 1);
}

INSTANTIATE_TEST_SUITE_P(Success,
                         TokenBasedEnrollmentUMASuccessTest,
                         testing::ValuesIn(test_cases));

INSTANTIATE_TEST_SUITE_P(Failure,
                         TokenBasedEnrollmentUMAFailureTest,
                         testing::ValuesIn(test_cases));

}  //  namespace ash
