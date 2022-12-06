// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

namespace em = enterprise_management;
using UserSegment = UserTypeByDeviceTypeMetricsProvider::UserSegment;
using ash::KioskLaunchController;
using ash::KioskSessionInitializedWaiter;
using ash::LoginScreenTestApi;
using ash::ScopedDeviceSettings;
using ash::WebKioskAppManager;
using testing::InvokeWithoutArgs;

const char kAccountId1[] = "dla1@example.com";
const char kDisplayName1[] = "display name 1";
const char kAppInstallUrl[] = "https://app.com/install";

absl::optional<em::PolicyData::MarketSegment> GetMarketSegment(
    policy::MarketSegment device_segment) {
  switch (device_segment) {
    case policy::MarketSegment::UNKNOWN:
      return absl::nullopt;
    case policy::MarketSegment::EDUCATION:
      return em::PolicyData::ENROLLED_EDUCATION;
    case policy::MarketSegment::ENTERPRISE:
      return em::PolicyData::ENROLLED_ENTERPRISE;
  }
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment(
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
      return absl::nullopt;
  }
  NOTREACHED();
  return absl::nullopt;
}

absl::optional<AccountId> GetPrimaryAccountId() {
  return AccountId::FromUserEmailGaiaId(
      ash::FakeGaiaMixin::kEnterpriseUser1,
      ash::FakeGaiaMixin::kEnterpriseUser1GaiaId);
}

void ProvideHistograms(bool should_emit_histograms_earlier) {
  // The purpose of the below call is to avoid a DCHECK failure in an unrelated
  // metrics provider, in |FieldTrialsProvider::ProvideCurrentSessionData()|.
  metrics::SystemProfileProto system_profile_proto;
  g_browser_process->metrics_service()
      ->GetDelegatingProviderForTesting()
      ->ProvideSystemProfileMetricsWithLogCreationTime(base::TimeTicks::Now(),
                                                       &system_profile_proto);
  if (!should_emit_histograms_earlier) {
    metrics::ChromeUserMetricsExtension uma_proto;
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->ProvideCurrentSessionData(&uma_proto);
  } else {
    g_browser_process->metrics_service()
        ->GetDelegatingProviderForTesting()
        ->OnDidCreateMetricsLog();
  }
}

class TestCase {
 public:
  TestCase(UserSegment user_segment,
           policy::MarketSegment device_segment,
           bool emit_histograms_earlier)
      : user_segment_(user_segment),
        device_segment_(device_segment),
        emit_histograms_earlier_(emit_histograms_earlier) {}

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

  absl::optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment()
      const {
    return ::GetMetricsLogSegment(user_segment_);
  }

  absl::optional<em::PolicyData::MarketSegment> GetMarketSegment() const {
    return ::GetMarketSegment(device_segment_);
  }

  bool GetShouldEmitEarlier() const { return emit_histograms_earlier_; }

  bool IsPublicSession() const {
    return GetUserSegment() == UserSegment::kManagedGuestSession;
  }

  bool IsKioskApp() const { return GetUserSegment() == UserSegment::kKioskApp; }

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
  bool emit_histograms_earlier_;
  bool uma_expected_{true};
};

TestCase UserCase(UserSegment user_segment,
                  policy::MarketSegment device_segment,
                  bool emit_histograms_earlier) {
  TestCase test_case(user_segment, device_segment, emit_histograms_earlier);
  return test_case;
}

TestCase MgsCase(policy::MarketSegment device_segment,
                 bool emit_histograms_earlier) {
  TestCase test_case(UserSegment::kManagedGuestSession, device_segment,
                     emit_histograms_earlier);
  return test_case;
}

TestCase KioskCase(policy::MarketSegment device_segment,
                   bool emit_histograms_earlier) {
  TestCase test_case(UserSegment::kKioskApp, device_segment,
                     emit_histograms_earlier);
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

  void SetUp() override {
    if (GetParam().GetShouldEmitEarlier()) {
      feature_list_.InitWithFeatures(
          {metrics::features::kEmitHistogramsEarlier}, {});
    } else {
      feature_list_.InitWithFeatures(
          {}, {metrics::features::kEmitHistogramsEarlier});
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
    policy_test_server_mixin_.UpdateExternalPolicy(
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
    // Add an account with DeviceLocalAccount::Type::TYPE_PUBLIC_SESSION.
    AddPublicSessionToDevicePolicy(kAccountId1);

    absl::optional<em::PolicyData::MarketSegment> market_segment =
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
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
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
    absl::optional<em::PolicyData::MetricsLogSegment> log_segment =
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
    auto* host = ash::LoginDisplayHost::default_host();
    ASSERT_TRUE(host);
    host->StartSignInScreen();
    auto* controller = ash::ExistingUserController::current_controller();
    ASSERT_TRUE(controller);

    ash::UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                  account_id_1_);
    user_context.SetPublicSessionLocale(std::string());
    user_context.SetPublicSessionInputMethod(std::string());
    controller->Login(user_context, ash::SigninSpecifics());
  }

  void PrepareAppLaunch() {
    std::vector<policy::DeviceLocalAccount> device_local_accounts = {
        policy::DeviceLocalAccount(
            policy::WebKioskAppBasicInfo(kAppInstallUrl, "", ""),
            kAppInstallUrl)};

    settings_ = std::make_unique<ScopedDeviceSettings>();
    int ui_update_count = LoginScreenTestApi::GetUiUpdateCount();
    policy::SetDeviceLocalAccounts(settings_->owner_settings_service(),
                                   device_local_accounts);
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
    if (IsSessionStarted())
      return;
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
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, ash::LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(), this,
      /*should_launch_browser=*/true, GetPrimaryAccountId(),
      /*include_initial_user=*/true,
      // Don't use EmbeddedPolicyTestServer because it does not support
      // customizing PolicyData.
      // TODO(crbug/1112885): Use EmbeddedPolicyTestServer when this is fixed.
      /*use_embedded_policy_server=*/false};
  policy::UserPolicyBuilder device_local_account_policy_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};

  const AccountId account_id_1_ =
      AccountId::FromUserEmail(GenerateDeviceLocalAccountUserId(
          kAccountId1,
          policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION));
  const AccountId account_id_2_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
          kAppInstallUrl,
          policy::DeviceLocalAccount::TYPE_WEB_KIOSK_APP));
  // Not strictly necessary, but makes kiosk tests run much faster.
  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_ =
      KioskLaunchController::SkipSplashScreenWaitForTesting();
  std::unique_ptr<ScopedDeviceSettings> settings_;
  base::test::ScopedFeatureList feature_list_;
};

// Flacky on CrOS (http://crbug.com/1248669).
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Uma DISABLED_Uma
#else
#define MAYBE_Uma Uma
#endif
IN_PROC_BROWSER_TEST_P(UserTypeByDeviceTypeMetricsProviderTest, MAYBE_Uma) {
  base::HistogramTester histogram_tester;

  SetDevicePolicy();

  // Simulate calling ProvideHistograms() prior to logging in.
  ProvideHistograms(GetParam().GetShouldEmitEarlier());

  // No metrics were recorded.
  histogram_tester.ExpectTotalCount(
      UserTypeByDeviceTypeMetricsProvider::GetHistogramNameForTesting(), 0);

  if (GetParam().IsPublicSession()) {
    StartPublicSession();
  } else if (GetParam().IsKioskApp()) {
    StartKioskApp();
  } else {
    LogInUser();
  }

  // Simulate calling ProvideHistograms() after logging in.
  ProvideHistograms(GetParam().GetShouldEmitEarlier());

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
        UserCase(UserSegment::kUnmanaged, policy::MarketSegment::UNKNOWN, true),
        UserCase(UserSegment::kK12, policy::MarketSegment::UNKNOWN, true),
        UserCase(UserSegment::kUniversity,
                 policy::MarketSegment::UNKNOWN,
                 true),
        UserCase(UserSegment::kNonProfit, policy::MarketSegment::UNKNOWN, true),
        UserCase(UserSegment::kEnterprise,
                 policy::MarketSegment::UNKNOWN,
                 true),
        UserCase(UserSegment::kUnmanaged,
                 policy::MarketSegment::EDUCATION,
                 true),
        UserCase(UserSegment::kK12, policy::MarketSegment::EDUCATION, true),
        UserCase(UserSegment::kUniversity,
                 policy::MarketSegment::EDUCATION,
                 true),
        UserCase(UserSegment::kNonProfit,
                 policy::MarketSegment::EDUCATION,
                 true),
        UserCase(UserSegment::kEnterprise,
                 policy::MarketSegment::EDUCATION,
                 true),
        UserCase(UserSegment::kUnmanaged,
                 policy::MarketSegment::ENTERPRISE,
                 true),
        UserCase(UserSegment::kK12, policy::MarketSegment::ENTERPRISE, true),
        UserCase(UserSegment::kUniversity,
                 policy::MarketSegment::ENTERPRISE,
                 true),
        UserCase(UserSegment::kNonProfit,
                 policy::MarketSegment::ENTERPRISE,
                 true),
        UserCase(UserSegment::kEnterprise,
                 policy::MarketSegment::ENTERPRISE,
                 true),
        KioskCase(policy::MarketSegment::UNKNOWN, true),
        KioskCase(policy::MarketSegment::EDUCATION, true),
        KioskCase(policy::MarketSegment::ENTERPRISE, true),
        MgsCase(policy::MarketSegment::UNKNOWN, true).DontExpectUmaOutput(),
        MgsCase(policy::MarketSegment::EDUCATION, true),
        MgsCase(policy::MarketSegment::ENTERPRISE, true),
        UserCase(UserSegment::kUnmanaged,
                 policy::MarketSegment::UNKNOWN,
                 false),
        UserCase(UserSegment::kK12, policy::MarketSegment::UNKNOWN, false),
        UserCase(UserSegment::kUniversity,
                 policy::MarketSegment::UNKNOWN,
                 false),
        UserCase(UserSegment::kNonProfit,
                 policy::MarketSegment::UNKNOWN,
                 false),
        UserCase(UserSegment::kEnterprise,
                 policy::MarketSegment::UNKNOWN,
                 false),
        UserCase(UserSegment::kUnmanaged,
                 policy::MarketSegment::EDUCATION,
                 false),
        UserCase(UserSegment::kK12, policy::MarketSegment::EDUCATION, false),
        UserCase(UserSegment::kUniversity,
                 policy::MarketSegment::EDUCATION,
                 false),
        UserCase(UserSegment::kNonProfit,
                 policy::MarketSegment::EDUCATION,
                 false),
        UserCase(UserSegment::kEnterprise,
                 policy::MarketSegment::EDUCATION,
                 false),
        UserCase(UserSegment::kUnmanaged,
                 policy::MarketSegment::ENTERPRISE,
                 false),
        UserCase(UserSegment::kK12, policy::MarketSegment::ENTERPRISE, false),
        UserCase(UserSegment::kUniversity,
                 policy::MarketSegment::ENTERPRISE,
                 false),
        UserCase(UserSegment::kNonProfit,
                 policy::MarketSegment::ENTERPRISE,
                 false),
        UserCase(UserSegment::kEnterprise,
                 policy::MarketSegment::ENTERPRISE,
                 false),
        KioskCase(policy::MarketSegment::UNKNOWN, false),
        KioskCase(policy::MarketSegment::EDUCATION, false),
        KioskCase(policy::MarketSegment::ENTERPRISE, false),
        MgsCase(policy::MarketSegment::UNKNOWN, false).DontExpectUmaOutput(),
        MgsCase(policy::MarketSegment::EDUCATION, false),
        MgsCase(policy::MarketSegment::ENTERPRISE, false)));
