// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include <optional>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/screens/mock_error_screen.h"
#include "chrome/browser/ash/login/ui/fake_login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr char kTestUserEmail[] = "user@test.org";
constexpr char kTestAuthCode[] = "test_auth_code";

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

namespace {

// Initialize network-related objects used by `EnrollmentScreen` and
// `ErrorScreen`.
class ScopedNetworkInitializer {
 public:
  ScopedNetworkInitializer() {
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    // Will be deleted in `network_portal_detector::Shutdown()`.
    MockNetworkPortalDetector* mock_network_portal_detector =
        new MockNetworkPortalDetector();
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector);

    EXPECT_CALL(*mock_network_portal_detector, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(testing::Return(false));
  }

  ~ScopedNetworkInitializer() { network_portal_detector::Shutdown(); }

 private:
  // Initializes NetworkHandler and required DBus clients.
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
};

}  // namespace

class EnrollmentScreenBaseTest : public testing::Test {
 public:
  EnrollmentScreenBaseTest(const EnrollmentScreenBaseTest&) = delete;
  EnrollmentScreenBaseTest& operator=(const EnrollmentScreenBaseTest&) = delete;

 protected:
  EnrollmentScreenBaseTest()
      : mock_error_screen_(mock_error_view_.AsWeakPtr()) {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    policy::EnrollmentRequisitionManager::Initialize();
  }

  ~EnrollmentScreenBaseTest() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

  // Creates the EnrollmentScreen and sets required parameters.
  void SetUpEnrollmentScreen(const policy::EnrollmentConfig& config) {
    enrollment_screen_ = std::make_unique<EnrollmentScreen>(
        mock_view_.AsWeakPtr(), &mock_error_screen_,
        base::BindRepeating(&EnrollmentScreenBaseTest::HandleScreenExit,
                            base::Unretained(this)));

    enrollment_screen_->SetEnrollmentConfig(config);
  }

  void SetUpEnrollmentScreen() {
    SetUpEnrollmentScreen(
        policy::EnrollmentConfig::GetPrescribedEnrollmentConfig());
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  void ExpectAttestationBasedEnrollmentAndReportSuccess() {
    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
        .WillOnce([this]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          enrollment_screen_->ShowEnrollmentStatusOnSuccess();
        });
  }

  void ExpectAttestationBasedEnrollmentAndReportFailure(
      policy::EnrollmentStatus status) {
    EXPECT_NE(status.enrollment_code(),
              policy::EnrollmentStatus::Code::kSuccess)
        << "Cannot not expect failure with a success code";

    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
        .Times(AnyNumber())
        .WillOnce([this, status]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          enrollment_screen_->OnEnrollmentError(status);
        });
  }

  void ExpectAttestationBasedEnrollmentAndReportFailure() {
    ExpectAttestationBasedEnrollmentAndReportFailure(
        policy::EnrollmentStatus::ForRegistrationError(
            policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  }

  void ExpectAttestationBasedEnrollmentAndReportFailureWithAutomaticFallback() {
    ExpectAttestationBasedEnrollmentAndReportFailure(
        policy::EnrollmentStatus::ForRegistrationError(
            policy::DeviceManagementStatus::
                DM_STATUS_SERVICE_DEVICE_NOT_FOUND));
  }

  void ExpectManualEnrollmentAndReportSuccess() {
    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAuthCode(kTestAuthCode))
        .WillOnce([this]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          enrollment_screen_->ShowEnrollmentStatusOnSuccess();
        });
  }

  void ExpectClearAuth() {
    EXPECT_CALL(mock_enrollment_launcher_, ClearAuth(_))
        .Times(AnyNumber())
        .WillRepeatedly(
            [](base::OnceClosure callback) { std::move(callback).Run(); });
  }

  void ExpectEnrollmentConfig(policy::EnrollmentConfig::Mode mode,
                              policy::EnrollmentConfig::AuthMechanism auth) {
    EXPECT_CALL(
        mock_view_,
        SetEnrollmentConfig(testing::AllOf(
            testing::Field(&policy::EnrollmentConfig::mode, mode),
            testing::Field(&policy::EnrollmentConfig::auth_mechanism, auth))));
  }

  void ExpectShowView() { EXPECT_CALL(mock_view_, Show()); }

  void ExpectShowViewWithLogin() {
    EXPECT_CALL(mock_view_, Show()).WillOnce([this]() {
      enrollment_screen_->OnLoginDone(
          kTestUserEmail, static_cast<int>(policy::LicenseType::kEnterprise),
          kTestAuthCode);
    });
  }

  void ShowEnrollmentScreen(bool suppress_jitter = false) {
    if (suppress_jitter) {
      // Remove jitter to enable deterministic testing.
      enrollment_screen_->retry_policy_.jitter_factor = 0;
    }
    enrollment_screen_->Show(&wizard_context());
  }

  void UserCancel() { enrollment_screen_->OnCancel(); }

  int GetEnrollmentScreenRetries() const {
    return enrollment_screen_->num_retries_;
  }

  const auto& last_screen_result() const { return last_screen_result_; }

  WizardContext& wizard_context() {
    return CHECK_DEREF(fake_login_display_host_.GetWizardContext());
  }

 private:
  void HandleScreenExit(EnrollmentScreen::Result screen_result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = screen_result;
  }

  void ExpectEnrollmentScreenIsEnrollmentStatusConsumer() {
    EXPECT_EQ(mock_enrollment_launcher_.status_consumer(),
              enrollment_screen_.get())
        << "EnrollmentScreen is not status consumer of EnrollmentLauncher";
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Must outlive `mock_error_screen_`.
  ScopedNetworkInitializer scoped_network;

  // Mocks must outlive `enrollment_screen_`.
  NiceMock<MockEnrollmentScreenView> mock_view_;
  NiceMock<MockErrorScreenView> mock_error_view_;
  // Network portal must be initialized before destroying.
  NiceMock<MockErrorScreen> mock_error_screen_;
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      scoped_enrollment_launcher_factory_override_{
          base::BindRepeating(FakeEnrollmentLauncher::Create,
                              &mock_enrollment_launcher_)};

  // Used by `enrollment_screen_`.
  ScopedStubInstallAttributes test_install_attributes_;

  // Used by `EnrollmentRequisitionManager`.
  TestingPrefServiceSimple pref_service_;

  // Used by `EnrollmentRequisitionManager`.
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  FakeLoginDisplayHost fake_login_display_host_;

  std::unique_ptr<EnrollmentScreen> enrollment_screen_;

  // The last result reported by `enrollment_screen_`.
  std::optional<EnrollmentScreen::Result> last_screen_result_;
};

class EnrollmentScreenRollbackFlowTest : public EnrollmentScreenBaseTest {
 protected:
  EnrollmentScreenRollbackFlowTest() { ConfigureRestoreAfterRollback(); }

  static const policy::EnrollmentConfig& rollback_config() {
    static policy::EnrollmentConfig config;
    config.mode = policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED;
    config.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    return config;
  }

  static const policy::EnrollmentConfig& rollback_fallback_config() {
    static policy::EnrollmentConfig config;
    config.mode =
        policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK;
    config.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    return config;
  }

 private:
  void ConfigureRestoreAfterRollback() {
    wizard_context().configuration.Set(configuration::kRestoreAfterRollback,
                                       true);
  }
};

TEST_F(EnrollmentScreenRollbackFlowTest, ShouldFinishEnrollmentScreen) {
  ExpectEnrollmentConfig(rollback_config().mode,
                         rollback_config().auth_mechanism);
  ExpectAttestationBasedEnrollmentAndReportSuccess();
  ExpectClearAuth();

  SetUpEnrollmentScreen();
  ShowEnrollmentScreen();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
}

TEST_F(EnrollmentScreenRollbackFlowTest,
       ShouldNotAutomaticallyRetryEnrollment) {
  ExpectEnrollmentConfig(rollback_config().mode,
                         rollback_config().auth_mechanism);
  ExpectAttestationBasedEnrollmentAndReportFailure();
  ExpectClearAuth();

  SetUpEnrollmentScreen();
  ShowEnrollmentScreen(/*suppress_jitter=*/true);

  FastForwardTime(base::Days(1));

  EXPECT_EQ(GetEnrollmentScreenRetries(), 0);
  EXPECT_FALSE(last_screen_result().has_value());
}

TEST_F(EnrollmentScreenRollbackFlowTest,
       ShouldAutomaticallyFallbackToManuallEnrollment) {
  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(rollback_config().mode,
                           rollback_config().auth_mechanism);
    ExpectShowView();
    ExpectAttestationBasedEnrollmentAndReportFailureWithAutomaticFallback();

    // Second view is shown for manual fallback.
    ExpectEnrollmentConfig(rollback_fallback_config().mode,
                           rollback_fallback_config().auth_mechanism);
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportSuccess();
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen();
  ShowEnrollmentScreen();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
}

TEST_F(EnrollmentScreenRollbackFlowTest,
       ShouldFallbackToManualEnrollmentOnUserAction) {
  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(rollback_config().mode,
                           rollback_config().auth_mechanism);
    ExpectShowView();
    ExpectAttestationBasedEnrollmentAndReportFailure();

    // Second view is shown for manual fallback. This should be triggered after
    // user decides to fallback.
    ExpectEnrollmentConfig(rollback_fallback_config().mode,
                           rollback_fallback_config().auth_mechanism);
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportSuccess();
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen();
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());

  UserCancel();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
}

}  // namespace ash
