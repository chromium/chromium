// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/class_management_enabled_metrics_provider.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"

namespace {

using ClassManagementEnabled =
    ClassManagementEnabledMetricsProvider::ClassManagementEnabled;
using testing::InvokeWithoutArgs;

constexpr std::string_view kClassManagementDisabled = "disabled";
constexpr std::string_view kClassManagementStudent = "student";
constexpr std::string_view kClassManagementTeacher = "teacher";
constexpr std::string_view kHistogramName = "ChromeOS.ClassManagementEnabled";

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
  explicit TestCase(std::string_view policy_value,
                    ClassManagementEnabled segment)
      : policy_value_(policy_value), segment_(segment) {}

  ClassManagementEnabled GetSegment() const { return segment_; }
  std::string_view GetPolicyValue() const { return policy_value_; }

 private:
  const std::string_view policy_value_;
  const ClassManagementEnabled segment_;
};

class ClassManagementEnabledMetricsProviderTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<TestCase> {
 protected:
  ClassManagementEnabledMetricsProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kClassManagementEnabledMetricsProvider);
  }
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
            policy::dm_protocol::kChromePublicAccountPolicyType,
            FakeGaiaMixin::kEnterpriseUser1,
            device_local_account_policy_.payload().SerializeAsString());
    session_manager_client()->set_device_local_account_policy(
        FakeGaiaMixin::kEnterpriseUser1,
        device_local_account_policy_.GetBlob());
  }

  void LogInUser() {
    std::unique_ptr<ash::ScopedUserPolicyUpdate> policy =
        logged_in_user_mixin_.GetUserPolicyMixin()->RequestPolicyUpdate();
    policy->policy_payload()
        ->mutable_subproto1()
        ->mutable_classmanagementenabled()
        ->set_value(GetParam().GetPolicyValue());
    logged_in_user_mixin_.LogInUser();
  }

  int GetExpectedUmaValue() {
    return static_cast<int>(GetParam().GetSegment());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};
  policy::UserPolicyBuilder device_local_account_policy_;
};

IN_PROC_BROWSER_TEST_P(ClassManagementEnabledMetricsProviderTest, Uma) {
  base::HistogramTester histogram_tester;

  SetDevicePolicy();

  // Simulate calling ProvideHistograms() prior to logging in.
  ProvideHistograms();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(kHistogramName, 0);

  LogInUser();

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms();

  histogram_tester.ExpectUniqueSample(kHistogramName, GetExpectedUmaValue(), 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ClassManagementEnabledMetricsProviderTest,
    testing::Values(
        TestCase(kClassManagementDisabled, ClassManagementEnabled::kDisabled),
        TestCase(kClassManagementStudent, ClassManagementEnabled::kStudent),
        TestCase(kClassManagementTeacher, ClassManagementEnabled::kTeacher)));
}  // namespace
