// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"

#include "base/logging.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_chromeos.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_store_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"

namespace {

namespace em = enterprise_management;
using UserSegment = UserTypeByDeviceTypeMetricsProvider::UserSegment;
using testing::InvokeWithoutArgs;

const char kAccountId1[] = "dla1@example.com";
const char kDisplayName1[] = "display name 1";

base::Optional<em::PolicyData::MarketSegment> GetMarketSegment(
    policy::MarketSegment device_segment) {
  switch (device_segment) {
    case policy::MarketSegment::UNKNOWN:
      return base::nullopt;
    case policy::MarketSegment::EDUCATION:
      return em::PolicyData::ENROLLED_EDUCATION;
    case policy::MarketSegment::ENTERPRISE:
      return em::PolicyData::ENROLLED_ENTERPRISE;
  }
  NOTREACHED();
  return base::nullopt;
}

base::Optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment(
    UserSegment user_segment) {
  switch (user_segment) {
    case UserSegment::kK12:
      return em::PolicyData::K12;
    case UserSegment::kUniversity:
      return em::PolicyData::UNIVERSITY;
    case UserSegment::kNonProfit:
      return em::PolicyData::NONPROFIT;
    case UserSegment::kEnterprise:
      return em::PolicyData::ENTERPRISE;
    case UserSegment::kUnmanaged:
    case UserSegment::kManagedGuestSession:
      return base::nullopt;
  }
  NOTREACHED();
  return base::nullopt;
}

base::Optional<AccountId> GetPrimaryAccountId() {
  return AccountId::FromUserEmailGaiaId(
      chromeos::FakeGaiaMixin::kEnterpriseUser1,
      chromeos::FakeGaiaMixin::kEnterpriseUser1GaiaId);
}

void ProvideCurrentSessionData() {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  metrics::ChromeUserMetricsExtension uma_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideCurrentSessionData(&uma_proto);
}

class TestCase {
 public:
  TestCase(const char* const name,
           UserSegment user_segment,
           policy::MarketSegment device_segment)
      : name_(name),
        user_segment_(user_segment),
        device_segment_(device_segment),
        is_public_session_(false),
        uma_expected_(true) {
    CHECK(name && *name) << "no test case name";
  }

  std::string GetTestName() const {
    std::string full_name = name_;

    full_name += "_";

    switch (user_segment_) {
      case UserSegment::kUnmanaged:
        full_name += "UnmanagedUser";
        break;
      case UserSegment::kK12:
        full_name += "K12User";
        break;
      case UserSegment::kUniversity:
        full_name += "UniversityUser";
        break;
      case UserSegment::kNonProfit:
        full_name += "NonProfitUser";
        break;
      case UserSegment::kEnterprise:
        full_name += "EnterpriseUser";
        break;
      case UserSegment::kManagedGuestSession:
        full_name += "ManagedGuestSession";
    }

    full_name += "_on_";

    switch (device_segment_) {
      case policy::MarketSegment::UNKNOWN:
        full_name += "UmanagedDevice";
        break;
      case policy::MarketSegment::EDUCATION:
        full_name += "EducationDevice";
        break;
      case policy::MarketSegment::ENTERPRISE:
        full_name += "EnterpriseDevice";
        break;
    }

    return full_name;
  }

  UserSegment GetUserSegment() const { return user_segment_; }

  policy::MarketSegment GetDeviceSegment() const { return device_segment_; }

  base::Optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment()
      const {
    return ::GetMetricsLogSegment(user_segment_);
  }

  base::Optional<em::PolicyData::MarketSegment> GetMarketSegment() const {
    return ::GetMarketSegment(device_segment_);
  }

  TestCase& EnablePublicSession() {
    is_public_session_ = true;
    return *this;
  }

  TestCase& DisablePublicSession() {
    is_public_session_ = false;
    return *this;
  }

  bool IsPublicSession() const { return is_public_session_; }

  TestCase& ExpectUmaOutput() {
    uma_expected_ = true;
    return *this;
  }

  TestCase& DontExpectUmaOutput() {
    uma_expected_ = false;
    return *this;
  }

  bool UmaOutputExpected() const { return uma_expected_; }

 private:
  const char* const name_;
  UserSegment user_segment_;
  policy::MarketSegment device_segment_;
  bool is_public_session_;
  bool uma_expected_;
};

TestCase MgsCase(const char* const name, policy::MarketSegment device_segment) {
  TestCase test_case(name, UserSegment::kManagedGuestSession, device_segment);
  test_case.EnablePublicSession();
  return test_case;
}

}  // namespace

class UserTypeByDeviceTypeMetricsProviderTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
  UserTypeByDeviceTypeMetricsProviderTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kUserTypeByDeviceTypeMetricsProvider);
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    LOG(INFO) << "UserTypeByDeviceTypeMetricsProviderTest::"
              << GetParam().GetTestName();
    InitializePolicy();
  }

 protected:
  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
    policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
        &device_local_account_policy_, kAccountId1, kDisplayName1);
    SetManagedSessionsEnabled(/* managed_sessions_enabled */ true);
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
    ASSERT_TRUE(local_policy_mixin_.server()->UpdatePolicy(
        policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
        device_local_account_policy_.payload().SerializeAsString()));
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    UploadDeviceLocalAccountPolicy();
    session_manager_client()->set_device_local_account_policy(
        kAccountId1, device_local_account_policy_.GetBlob());
  }

  void SetDevicePolicy() {
    UploadAndInstallDeviceLocalAccountPolicy();
    // Add an account with DeviceLocalAccount::Type::TYPE_PUBLIC_SESSION.
    AddPublicSessionToDevicePolicy(kAccountId1);

    base::Optional<em::PolicyData::MarketSegment> market_segment =
        GetParam().GetMarketSegment();
    if (market_segment) {
      device_policy()->policy_data().set_market_segment(market_segment.value());
      RefreshDevicePolicy();
    }
    WaitForPolicy();
  }

  void AddPublicSessionToDevicePolicy(const std::string& username) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    policy::DeviceLocalAccountTestHelper::AddPublicSession(&proto, username);
    RefreshDevicePolicy();
    ASSERT_TRUE(local_policy_mixin_.UpdateDevicePolicy(proto));
  }

  void SetManagedSessionsEnabled(bool managed_sessions_enabled) {
    device_local_account_policy_.payload()
        .mutable_devicelocalaccountmanagedsessionenabled()
        ->set_value(managed_sessions_enabled);
    UploadDeviceLocalAccountPolicy();
  }

  void WaitForDisplayName(const std::string& user_id,
                          const std::string& expected_display_name) {
    policy::DictionaryLocalStateValueWaiter("UserDisplayName",
                                            expected_display_name, user_id)
        .Wait();
  }

  void WaitForPolicy() {
    // Wait for the display name becoming available as that indicates
    // device-local account policy is fully loaded, which is a prerequisite for
    // successful login.
    WaitForDisplayName(account_id_1_.GetUserEmail(), kDisplayName1);
  }

  void LogInUser() {
    base::Optional<em::PolicyData::MetricsLogSegment> log_segment =
        GetParam().GetMetricsLogSegment();
    if (log_segment) {
      logged_in_user_mixin_.GetUserPolicyMixin()
          ->RequestPolicyUpdate()
          ->policy_data()
          ->set_metrics_log_segment(log_segment.value());
    }
    logged_in_user_mixin_.LogInUser();
  }

  void StartPublicSession() {
    StartPublicSessionLogin();
    WaitForSessionStart();
  }

  void StartPublicSessionLogin() {
    // Start login into the device-local account.
    chromeos::LoginDisplayHost* host =
        chromeos::LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    host->StartSignInScreen();
    chromeos::ExistingUserController* controller =
        chromeos::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    chromeos::UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                       account_id_1_);
    user_context.SetPublicSessionLocale(std::string());
    user_context.SetPublicSessionInputMethod(std::string());
    controller->Login(user_context, chromeos::SigninSpecifics());
  }

  void WaitForSessionStart() {
    if (IsSessionStarted())
      return;
    chromeos::WizardController::SkipPostLoginScreensForTesting();
    chromeos::test::WaitForPrimaryUserSessionStart();
  }

  bool IsSessionStarted() {
    return session_manager::SessionManager::Get()->IsSessionStarted();
  }

  int GetExpectedUmaValue() {
    return UserTypeByDeviceTypeMetricsProvider::ConstructUmaValue(
        GetParam().GetUserSegment(), GetParam().GetDeviceSegment());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, chromeos::LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(), this,
      /*should_launch_browser=*/true, GetPrimaryAccountId(),
      /*include_initial_user=*/true,
      // Don't use LocalPolicyTestServer because it does not support customizing
      // PolicyData.
      // TODO(crbug/1112885): Use LocalPolicyTestServer when this is fixed.
      /*use_local_policy_server=*/false};
  policy::UserPolicyBuilder device_local_account_policy_;
  chromeos::LocalPolicyTestServerMixin local_policy_mixin_{&mixin_host_};

  const AccountId account_id_1_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId1,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));
};

IN_PROC_BROWSER_TEST_P(UserTypeByDeviceTypeMetricsProviderTest, Uma) {
  base::HistogramTester histogram_tester;

  SetDevicePolicy();

  // Simulate calling ProvideCurrentSessionData() prior to logging in.
  ProvideCurrentSessionData();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      UserTypeByDeviceTypeMetricsProvider::GetHistogramNameForTesting(), 0);

  if (GetParam().IsPublicSession()) {
    StartPublicSession();
  } else {
    LogInUser();
  }

  // Simulate calling ProvideCurrentSessionData() after logging in.
  ProvideCurrentSessionData();

  if (GetParam().UmaOutputExpected()) {
    histogram_tester.ExpectUniqueSample(
        UserTypeByDeviceTypeMetricsProvider::GetHistogramNameForTesting(),
        GetExpectedUmaValue(), 1);
  } else {
    // No metrics were recorded.
    histogram_tester.ExpectTotalCount(
        UserTypeByDeviceTypeMetricsProvider::GetHistogramNameForTesting(), 0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    UserTypeByDeviceTypeMetricsProviderTest,
    testing::Values(
        TestCase("Uma",
                 UserSegment::kUnmanaged,
                 policy::MarketSegment::UNKNOWN),
        TestCase("Uma", UserSegment::kK12, policy::MarketSegment::UNKNOWN),
        TestCase("Uma",
                 UserSegment::kUniversity,
                 policy::MarketSegment::UNKNOWN),
        TestCase("Uma",
                 UserSegment::kNonProfit,
                 policy::MarketSegment::UNKNOWN),
        TestCase("Uma",
                 UserSegment::kEnterprise,
                 policy::MarketSegment::UNKNOWN),
        TestCase("Uma",
                 UserSegment::kUnmanaged,
                 policy::MarketSegment::EDUCATION),
        TestCase("Uma", UserSegment::kK12, policy::MarketSegment::EDUCATION),
        TestCase("Uma",
                 UserSegment::kUniversity,
                 policy::MarketSegment::EDUCATION),
        TestCase("Uma",
                 UserSegment::kNonProfit,
                 policy::MarketSegment::EDUCATION),
        TestCase("Uma",
                 UserSegment::kEnterprise,
                 policy::MarketSegment::EDUCATION),
        TestCase("Uma",
                 UserSegment::kUnmanaged,
                 policy::MarketSegment::ENTERPRISE),
        TestCase("Uma", UserSegment::kK12, policy::MarketSegment::ENTERPRISE),
        TestCase("Uma",
                 UserSegment::kUniversity,
                 policy::MarketSegment::ENTERPRISE),
        TestCase("Uma",
                 UserSegment::kNonProfit,
                 policy::MarketSegment::ENTERPRISE),
        TestCase("Uma",
                 UserSegment::kEnterprise,
                 policy::MarketSegment::ENTERPRISE),
        MgsCase("Uma", policy::MarketSegment::UNKNOWN).DontExpectUmaOutput(),
        MgsCase("Uma", policy::MarketSegment::EDUCATION),
        MgsCase("Uma", policy::MarketSegment::ENTERPRISE)));
