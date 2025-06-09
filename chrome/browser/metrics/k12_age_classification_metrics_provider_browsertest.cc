// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/k12_age_classification_metrics_provider.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"

namespace {

namespace em = enterprise_management;
using K12AgeClassificationSegment =
    K12AgeClassificationMetricsProvider::K12AgeClassificationSegment;
using testing::InvokeWithoutArgs;

constexpr char kAccountId1[] = "dla1@example.com";

std::optional<em::PolicyData::K12AgeClassificationMetricsLogSegment>
GetK12AgeClassificationMetricsLogSegment(K12AgeClassificationSegment segment) {
  switch (segment) {
    case K12AgeClassificationSegment::kAgeUnder18:
      return em::PolicyData::AGE_UNDER18;
    case K12AgeClassificationSegment::kAgeEqualOrOver18:
      return em::PolicyData::AGE_EQUAL_OR_OVER18;
    case K12AgeClassificationSegment::kAgeUnspecified:
      [[fallthrough]];
    default:
      return em::PolicyData::AGE_UNSPECIFIED;
  }
}

void ProvideHistograms() {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  // Downstream functions do not use system_profile_proto so there is no risk of
  // UAF.
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->OnDidCreateMetricsLog();
}

class TestCase {
 public:
  explicit TestCase(K12AgeClassificationSegment segment) : segment_(segment) {}

  std::string GetTestName() const {
    switch (segment_) {
      case K12AgeClassificationSegment::kAgeUnder18:
        return "AgeUnder18";
      case K12AgeClassificationSegment::kAgeEqualOrOver18:
        return "AgeEqualOrOver18";
      case K12AgeClassificationSegment::kAgeUnspecified:
        return "AgeUnspecified";
    }
  }

  K12AgeClassificationSegment GetSegment() const { return segment_; }

  std::optional<em::PolicyData::K12AgeClassificationMetricsLogSegment>
  GetK12AgeClassificationMetricsLogSegment() const {
    return ::GetK12AgeClassificationMetricsLogSegment(segment_);
  }

 private:
  const K12AgeClassificationSegment segment_;
};

class K12AgeClassificationMetricsProviderTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<TestCase> {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    InitializePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
  }

  void SetDevicePolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
    logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
        ->UpdateExternalPolicy(
            policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
            device_local_account_policy_.payload().SerializeAsString());
    session_manager_client()->set_device_local_account_policy(
        kAccountId1, device_local_account_policy_.GetBlob());
  }

  void LogInUser() {
    std::optional<em::PolicyData::K12AgeClassificationMetricsLogSegment>
        log_segment = GetParam().GetK12AgeClassificationMetricsLogSegment();
    if (log_segment) {
      logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
          ->SetK12AgeClassificationMetricsLogSegment(log_segment.value());
    }
    logged_in_user_mixin_.LogInUser();
  }

  int GetExpectedUmaValue() {
    return static_cast<int>(GetParam().GetSegment());
  }

 private:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};
  policy::UserPolicyBuilder device_local_account_policy_;
};

IN_PROC_BROWSER_TEST_P(K12AgeClassificationMetricsProviderTest, Uma) {
  base::HistogramTester histogram_tester;

  SetDevicePolicy();

  // Simulate calling ProvideHistograms() prior to logging in.
  ProvideHistograms();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      K12AgeClassificationMetricsProvider::kHistogramName, 0);

  LogInUser();

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(
      K12AgeClassificationMetricsProvider::kHistogramName,
      GetExpectedUmaValue(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    K12AgeClassificationMetricsProviderTest,
    testing::Values(TestCase(K12AgeClassificationSegment::kAgeUnder18),
                    TestCase(K12AgeClassificationSegment::kAgeEqualOrOver18),
                    TestCase(K12AgeClassificationSegment::kAgeUnspecified)));
}  // namespace
