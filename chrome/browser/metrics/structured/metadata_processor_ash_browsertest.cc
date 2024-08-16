// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/metadata_processor_ash.h"

#include <optional>
#include <utility>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/feature_list.h"
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
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_service.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {
namespace em = enterprise_management;
using ash::KioskSessionInitializedWaiter;
using ash::LoginScreenTestApi;
using ash::ScopedDeviceSettings;
using ash::WebKioskAppManager;
using testing::InvokeWithoutArgs;
using StructuredEventProto = metrics::StructuredEventProto;
using StructuredDataProto = metrics::StructuredDataProto;
using PrimaryUserSegment = StructuredEventProto::PrimaryUserSegment;
using DeviceSegment = StructuredDataProto::DeviceSegment;

const char kAccountId1[] = "dla1@example.com";
const char kDisplayName1[] = "display name 1";
const char kAppInstallUrl[] = "https://app.com/install";

// The name hash of "CrOSEvents".
constexpr uint64_t kCrosEventsHash = UINT64_C(12657197978410187837);

// The name hash of "chrome::CrOSEvents::NoMetricsEvent".
constexpr uint64_t kNoMetricsEventHash = UINT64_C(5106854608989380457);

std::optional<em::PolicyData::MarketSegment> GetMarketSegment(
    DeviceSegment device_segment) {
  switch (device_segment) {
    case StructuredDataProto::UNKNOWN:
    case StructuredDataProto::CONSUMER:
      return std::nullopt;
    case StructuredDataProto::EDUCATION:
      return em::PolicyData::ENROLLED_EDUCATION;
    case StructuredDataProto::ENTERPRISE:
      return em::PolicyData::ENROLLED_ENTERPRISE;
  }
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

std::optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment(
    PrimaryUserSegment primary_user_segment) {
  switch (primary_user_segment) {
    case StructuredEventProto::K12:
      return em::PolicyData::K12;
    case StructuredEventProto::UNIVERSITY:
      return em::PolicyData::UNIVERSITY;
    case StructuredEventProto::NON_PROFIT:
      return em::PolicyData::NONPROFIT;
    case StructuredEventProto::ENTERPRISE_ORGANIZATION:
      return em::PolicyData::ENTERPRISE;
    case StructuredEventProto::UNMANAGED:
    case StructuredEventProto::KIOS_APP:
    case StructuredEventProto::MANAGED_GUEST_SESSION:
    case StructuredEventProto::DEMO_MODE:
    case StructuredEventProto::UNKNOWN_PRIMARY_USER_TYPE:
      return std::nullopt;
  }
  NOTREACHED_IN_MIGRATION();
  return std::nullopt;
}

}  // namespace

namespace metrics::structured {

namespace {
class TestCase {
 public:
  TestCase(PrimaryUserSegment primary_user_segment,
           DeviceSegment device_segment)
      : primary_user_segment_(primary_user_segment),
        device_segment_(device_segment) {}

  PrimaryUserSegment GetPrimaryUserSegement() const {
    return primary_user_segment_;
  }

  DeviceSegment GetDeviceSegment() const { return device_segment_; }

  std::optional<em::PolicyData::MetricsLogSegment> GetMetricsLogSegment()
      const {
    return ::GetMetricsLogSegment(primary_user_segment_);
  }

  std::optional<em::PolicyData::MarketSegment> GetMarketSegment() const {
    return ::GetMarketSegment(device_segment_);
  }

  bool IsPublicSession() const {
    return primary_user_segment_ == StructuredEventProto::MANAGED_GUEST_SESSION;
  }

  bool IsKioskApp() const {
    return primary_user_segment_ == StructuredEventProto::KIOS_APP;
  }

  bool IsDemoSession() const {
    return primary_user_segment_ == StructuredEventProto::DEMO_MODE;
  }

 private:
  PrimaryUserSegment primary_user_segment_;
  DeviceSegment device_segment_;
};

TestCase UserCase(PrimaryUserSegment user_segment,
                  DeviceSegment device_segment) {
  TestCase test_case(user_segment, device_segment);
  return test_case;
}

TestCase MgsCase(DeviceSegment device_segment) {
  TestCase test_case(StructuredEventProto::MANAGED_GUEST_SESSION,
                     device_segment);
  return test_case;
}

TestCase KioskCase(DeviceSegment device_segment) {
  TestCase test_case(StructuredEventProto::KIOS_APP, device_segment);
  return test_case;
}

TestCase DemoModeCase() {
  TestCase test_case(StructuredEventProto::DEMO_MODE,
                     StructuredDataProto::ENTERPRISE);
  return test_case;
}
}  // namespace

class MetadataProcessorTest : public policy::DevicePolicyCrosBrowserTest,
                              public testing::WithParamInterface<TestCase> {
 public:
  MetadataProcessorTest() {
    if (GetParam().IsDemoSession()) {
      device_state_.SetState(
          ash::DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE);
    }
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
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

  void SetUp() override {
    // These tests are only applicable if structured metrics service is enabled.
    if (!base::FeatureList::IsEnabled(kEnabledStructuredMetricsService)) {
      GTEST_SKIP() << "Skipping test: Structured Metrics Service and CrOS "
                      "Events must be enabled";
    }
    policy::DevicePolicyCrosBrowserTest::SetUp();
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

  void Wait() { base::RunLoop().RunUntilIdle(); }

 protected:
  StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_,
                                                   /*setup_profile=*/false};

  // Helper function to find an event in an already build Uma Proto.
  static std::optional<StructuredEventProto> FindEvent(
      const StructuredDataProto& structured_data,
      uint64_t project_name_hash,
      uint64_t event_name_hash) {
    for (const auto& event : structured_data.events()) {
      if (event.project_name_hash() == project_name_hash &&
          event.event_name_hash() == event_name_hash) {
        return event;
      }
    }
    return std::nullopt;
  }

 private:
  ash::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_, /*test_base=*/this, embedded_test_server(),
      ash::LoggedInUserMixin::LogInType::kManaged};
  policy::UserPolicyBuilder device_local_account_policy_;

  const AccountId account_id_1_ =
      AccountId::FromUserEmail(policy::GenerateDeviceLocalAccountUserId(
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

// TODO(b/303139902) re-enable test once flakiness is addressed.
IN_PROC_BROWSER_TEST_P(MetadataProcessorTest, DISABLED_UserMetadata) {
  using NoMetricsEvent = events::v2::cr_os_events::NoMetricsEvent;

  SetDevicePolicy();

  if (GetParam().IsPublicSession()) {
    StartPublicSession();
  } else if (GetParam().IsKioskApp()) {
    StartKioskApp();
  } else if (GetParam().IsDemoSession()) {
    StartDemoSession();
  } else {
    LogInUser();
  }

  structured_metrics_mixin_.UpdateRecordingState(true);

  structured_metrics_mixin_.WaitUntilKeysReady();

  ASSERT_TRUE(structured_metrics_mixin_.GetRecorder()->CanProvideMetrics());

  StructuredMetricsClient::Record(std::move(NoMetricsEvent()));

  Wait();

  structured_metrics_mixin_.WaitUntilEventRecorded(kCrosEventsHash,
                                                   kNoMetricsEventHash);

  structured_metrics_mixin_.GetService()->Flush(
      metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  std::unique_ptr<ChromeUserMetricsExtension> uma_proto =
      structured_metrics_mixin_.GetUmaProto();
  ASSERT_NE(uma_proto.get(), nullptr);

  const StructuredDataProto& structured_data = uma_proto->structured_data();
  EXPECT_EQ(structured_data.device_segment(), GetParam().GetDeviceSegment());

  std::optional<StructuredEventProto> event =
      FindEvent(structured_data, kCrosEventsHash, kNoMetricsEventHash);
  ASSERT_TRUE(event.has_value());

  EXPECT_EQ(event->event_sequence_metadata().primary_user_segment(),
            GetParam().GetPrimaryUserSegement());
}

INSTANTIATE_TEST_SUITE_P(
    ,
    MetadataProcessorTest,
    testing::Values(
        UserCase(StructuredEventProto::UNMANAGED,
                 StructuredDataProto::CONSUMER),
        UserCase(StructuredEventProto::NON_PROFIT,
                 StructuredDataProto::CONSUMER),
        UserCase(StructuredEventProto::UNIVERSITY,
                 StructuredDataProto::EDUCATION),
        UserCase(StructuredEventProto::NON_PROFIT,
                 StructuredDataProto::EDUCATION),
        UserCase(StructuredEventProto::K12, StructuredDataProto::ENTERPRISE),
        UserCase(StructuredEventProto::ENTERPRISE_ORGANIZATION,
                 StructuredDataProto::ENTERPRISE),
        metrics::structured::KioskCase(StructuredDataProto::ENTERPRISE),
        MgsCase(StructuredDataProto::CONSUMER),
        DemoModeCase()));

}  // namespace metrics::structured
