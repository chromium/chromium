// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"

#include <optional>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_mode/kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
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
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"

namespace {

namespace em = enterprise_management;
using UserSegment = UserTypeByDeviceTypeMetricsProvider::UserSegment;
using ash::KioskSessionInitializedWaiter;
using ash::LoginScreenTestApi;
using ash::ScopedDeviceSettings;
using ash::WebKioskAppManager;
using testing::InvokeWithoutArgs;

const char kAccountId1[] = "dla1@example.com";
const char kDisplayName1[] = "display name 1";
const char kAppInstallUrl[] = "https://app.com/install";

std::optional<em::PolicyData::MarketSegment> GetMarketSegment(
    policy::MarketSegment device_segment) {
  switch (device_segment) {
    case policy::MarketSegment::UNKNOWN:
      return std::nullopt;
    case policy::MarketSegment::EDUCATION:
      return em::PolicyData::ENROLLED_EDUCATION;
    case policy::MarketSegment::ENTERPRISE:
      return em::PolicyData::ENROLLED_ENTERPRISE;
  }
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment(
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
    case UserSegment::kKioskApp:
    case UserSegment::kManagedGuestSession:
    case UserSegment::kDemoMode:
      return std::nullopt;
  }
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

void ProvideHistograms() {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
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
  TestCase(UserSegment user_segment, policy::MarketSegment device_segment)
      : user_segment_(user_segment), device_segment_(device_segment) {}

  std::string GetTestName() const {
    std::string test_name = "";

    switch (user_segment_) {
      case UserSegment::kUnmanaged:
        test_name += "UnmanagedUser";
        break;
      case UserSegment::kK12:
        test_name += "K12User";
        break;
      case UserSegment::kUniversity:
        test_name += "UniversityUser";
        break;
      case UserSegment::kNonProfit:
        test_name += "NonProfitUser";
        break;
      case UserSegment::kEnterprise:
        test_name += "EnterpriseUser";
        break;
      case UserSegment::kKioskApp:
        test_name += "KioskApp";
        break;
      case UserSegment::kManagedGuestSession:
        test_name += "ManagedGuestSession";
        break;
      case UserSegment::kDemoMode:
        test_name += "DemoMode";
        break;
    }

    test_name += "_on_";

    switch (device_segment_) {
      case policy::MarketSegment::UNKNOWN:
        test_name += "UmanagedDevice";
        break;
      case policy::MarketSegment::EDUCATION:
        test_name += "EducationDevice";
        break;
      case policy::MarketSegment::ENTERPRISE:
        test_name += "EnterpriseDevice";
        break;
    }

    return test_name;
  }

  UserSegment GetUserSegment() const { return user_segment_; }

  policy::MarketSegment GetDeviceSegment() const { return device_segment_; }

  std::optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment()
      const {
    return ::GetMetricsLogSegment(user_segment_);
  }

  std::optional<em::PolicyData::MarketSegment> GetMarketSegment() const {
    return ::GetMarketSegment(device_segment_);
  }

  bool IsPublicSession() const {
    return GetUserSegment() == UserSegment::kManagedGuestSession;
  }

  bool IsKioskApp() const { return GetUserSegment() == UserSegment::kKioskApp; }

  bool IsDemoSession() const {
    return GetUserSegment() == UserSegment::kDemoMode;
  }

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
  UserSegment user_segment_;
  policy::MarketSegment device_segment_;
  bool uma_expected_{true};
};

TestCase UserCase(UserSegment user_segment,
                  policy::MarketSegment device_segment) {
  TestCase test_case(user_segment, device_segment);
  return test_case;
}

TestCase MgsCase(policy::MarketSegment device_segment) {
  TestCase test_case(UserSegment::kManagedGuestSession, device_segment);
  return test_case;
}

TestCase KioskCase(policy::MarketSegment device_segment) {
  TestCase test_case(UserSegment::kKioskApp, device_segment);
  return test_case;
}

TestCase DemoModeCase() {
  TestCase test_case(UserSegment::kDemoMode, policy::MarketSegment::ENTERPRISE);
  return test_case;
}
}  // namespace

class UserTypeByDeviceTypeMetricsProviderTest
    : public policy::DevicePolicyCrosBrowserTest,
      public testing::WithParamInterface<TestCase> {
 public:
  UserTypeByDeviceTypeMetricsProviderTest() {
    if (GetParam().IsDemoSession()) {
      device_state_.SetState(
          ash::DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    LOG(INFO) << "UserTypeByDeviceTypeMetricsProviderTest::"
              << GetParam().GetTestName();
    InitializePolicy();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kOobeSkipPostLogin);
    DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void TearDownOnMainThread() override {
    settings_.reset();
    policy::DevicePolicyCrosBrowserTest::TearDownOnMainThread();
  }

 protected:
  void InitializePolicy() {
    device_policy()->policy_data().set_public_key_version(1);
    policy::DeviceLocalAccountTestHelper::SetupDeviceLocalAccount(
        &device_local_account_policy_, kAccountId1, kDisplayName1);
  }

  void BuildDeviceLocalAccountPolicy() {
    device_local_account_policy_.SetDefaultSigningKey();
    device_local_account_policy_.Build();
  }

  void UploadDeviceLocalAccountPolicy() {
    BuildDeviceLocalAccountPolicy();
    logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
        ->UpdateExternalPolicy(
            policy::dm_protocol::kChromePublicAccountPolicyType, kAccountId1,
            device_local_account_policy_.payload().SerializeAsString());
  }

  void UploadAndInstallDeviceLocalAccountPolicy() {
    UploadDeviceLocalAccountPolicy();
    session_manager_client()->set_device_local_account_policy(
        kAccountId1, device_local_account_policy_.GetBlob());
  }

  void SetDevicePolicy() {
    UploadAndInstallDeviceLocalAccountPolicy();
    // Add an account with DeviceLocalAccountType::kPublicSession.
    AddPublicSessionToDevicePolicy(kAccountId1);

    std::optional<em::PolicyData::MarketSegment> market_segment =
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
    logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
        ->UpdateDevicePolicy(proto);
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
    std::optional<em::PolicyData::MetricsLogSegment> log_segment =
        GetParam().GetMetricsLogSegment();
    if (log_segment) {
      logged_in_user_mixin_.GetEmbeddedPolicyTestServerMixin()
          ->SetMetricsLogSegment(log_segment.value());
    }
    logged_in_user_mixin_.LogInUser();
  }

  void StartPublicSession() {
    StartPublicSessionLogin();
    WaitForSessionStart();
  }

  void StartPublicSessionLogin() {
    // Start login into the device-local account.
    auto* host = ash::LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    host->StartSignInScreen();
    auto* controller = ash::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    ash::UserContext user_context(user_manager::UserType::kPublicAccount,
                                  account_id_1_);
    user_context.SetPublicSessionLocale(std::string());
    user_context.SetPublicSessionInputMethod(std::string());
    controller->Login(user_context, ash::SigninSpecifics());
  }

  void StartDemoSession() {
    // Set Demo Mode config to online.
    ash::DemoSession::SetDemoConfigForTesting(
        ash::DemoSession::DemoModeConfig::kOnline);
    ash::test::LockDemoDeviceInstallAttributes();
    ash::DemoSession::StartIfInDemoMode();

    // Start the public session, Demo Mode is a special public session.
    StartPublicSession();
  }

  void PrepareAppLaunch() {
    std::vector<policy::DeviceLocalAccount> device_local_accounts = {
        policy::DeviceLocalAccount(
            policy::DeviceLocalAccount::EphemeralMode::kUnset,
            policy::WebKioskAppBasicInfo(kAppInstallUrl, "", ""),
            kAppInstallUrl)};

    settings_ = std::make_unique<ScopedDeviceSettings>();
    int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
    policy::SetDeviceLocalAccountsForTesting(
        settings_->owner_settings_service(), device_local_accounts);
    // Wait for the Kiosk App configuration to reload.
    LoginScreenTestApi::WaitForUiUpdate(ui_update_count);
  }

  bool LaunchApp() {
    return LoginScreenTestApi::LaunchApp(
        WebKioskAppManager::Get()->GetAppByAccountId(account_id_2_)->app_id());
  }

  void StartKioskApp() {
    PrepareAppLaunch();
    LaunchApp();
    KioskSessionInitializedWaiter().Wait();
  }

  void WaitForSessionStart() {
    if (IsSessionStarted()) {
      return;
    }
    ash::test::WaitForPrimaryUserSessionStart();
  }

  bool IsSessionStarted() {
    return session_manager::SessionManager::Get()->IsSessionStarted();
  }

  int GetExpectedUmaValue() {
    return UserTypeByDeviceTypeMetricsProvider::ConstructUmaValue(
        GetParam().GetUserSegment(), GetParam().GetDeviceSegment());
  }

 private:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};
  policy::UserPolicyBuilder device_local_account_policy_;

  const AccountId account_id_1_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId1,
          policy::DeviceLocalAccountType::kPublicSession));
  const AccountId account_id_2_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          kAppInstallUrl,
          policy::DeviceLocalAccountType::kWebKioskApp));
  // Not strictly necessary, but makes kiosk tests run much faster.
  base::AutoReset<bool> skip_splash_wait_override_ =
      ash::KioskTestHelper::SkipSplashScreenWait();
  std::unique_ptr<ScopedDeviceSettings> settings_;
};

// Flaky on CrOS (http://crbug.com/1248669).
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Uma DISABLED_Uma
#else
#define MAYBE_Uma Uma
#endif
IN_PROC_BROWSER_TEST_P(UserTypeByDeviceTypeMetricsProviderTest, MAYBE_Uma) {
  base::HistogramTester histogram_tester;

  SetDevicePolicy();

  // Simulate calling ProvideHistograms() prior to logging in.
  ProvideHistograms();

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      UserTypeByDeviceTypeMetricsProvider::GetHistogramNameForTesting(), 0);

  if (GetParam().IsPublicSession()) {
    StartPublicSession();
  } else if (GetParam().IsKioskApp()) {
    StartKioskApp();
  } else if (GetParam().IsDemoSession()) {
    StartDemoSession();
  } else {
    LogInUser();
  }

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms();

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
        UserCase(UserSegment::kUnmanaged, policy::MarketSegment::UNKNOWN),
        UserCase(UserSegment::kK12, policy::MarketSegment::UNKNOWN),
        UserCase(UserSegment::kUniversity, policy::MarketSegment::UNKNOWN),
        UserCase(UserSegment::kNonProfit, policy::MarketSegment::UNKNOWN),
        UserCase(UserSegment::kEnterprise, policy::MarketSegment::UNKNOWN),
        UserCase(UserSegment::kUnmanaged, policy::MarketSegment::EDUCATION),
        UserCase(UserSegment::kK12, policy::MarketSegment::EDUCATION),
        UserCase(UserSegment::kUniversity, policy::MarketSegment::EDUCATION),
        UserCase(UserSegment::kNonProfit, policy::MarketSegment::EDUCATION),
        UserCase(UserSegment::kEnterprise, policy::MarketSegment::EDUCATION),
        UserCase(UserSegment::kUnmanaged, policy::MarketSegment::ENTERPRISE),
        UserCase(UserSegment::kK12, policy::MarketSegment::ENTERPRISE),
        UserCase(UserSegment::kUniversity, policy::MarketSegment::ENTERPRISE),
        UserCase(UserSegment::kNonProfit, policy::MarketSegment::ENTERPRISE),
        UserCase(UserSegment::kEnterprise, policy::MarketSegment::ENTERPRISE),
        KioskCase(policy::MarketSegment::UNKNOWN),
        KioskCase(policy::MarketSegment::EDUCATION),
        KioskCase(policy::MarketSegment::ENTERPRISE),
        MgsCase(policy::MarketSegment::UNKNOWN).DontExpectUmaOutput(),
        MgsCase(policy::MarketSegment::EDUCATION),
        MgsCase(policy::MarketSegment::ENTERPRISE),
        DemoModeCase()));
