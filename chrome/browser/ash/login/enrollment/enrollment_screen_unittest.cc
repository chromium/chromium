// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/screens/mock_error_screen.h"
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

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::NiceMock;

class EnrollmentScreenUnitTest : public testing::Test {
 public:
  enum class AttestationEnrollmentStatus {
    SUCCESS,
    DEVICE_NOT_SETUP_FOR_ZERO_TOUCH,
    DMSERVER_ERROR
  };

  EnrollmentScreenUnitTest() = default;

  EnrollmentScreenUnitTest(const EnrollmentScreenUnitTest&) = delete;
  EnrollmentScreenUnitTest& operator=(const EnrollmentScreenUnitTest&) = delete;

  // Creates the EnrollmentScreen and sets required parameters.
  void SetUpEnrollmentScreen(const policy::EnrollmentConfig& config) {
    enrollment_screen_ = std::make_unique<EnrollmentScreen>(
        mock_view_.AsWeakPtr(), mock_error_screen_.get(),
        base::BindRepeating(&EnrollmentScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));

    enrollment_screen_->SetEnrollmentConfig(config);
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  // testing::Test:
  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    policy::EnrollmentRequisitionManager::Initialize();

    // Initialize network-related objects which are needed for the
    // `enrollment_screen_`.
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    // Will be deleted in `network_portal_detector::Shutdown()`.
    MockNetworkPortalDetector* mock_network_portal_detector =
        new MockNetworkPortalDetector();
    network_portal_detector::SetNetworkPortalDetector(
        mock_network_portal_detector);
    mock_error_screen_ = std::make_unique<NiceMock<MockErrorScreen>>(
        mock_error_view_.AsWeakPtr());

    EXPECT_CALL(*mock_network_portal_detector, IsEnabled())
        .Times(AnyNumber())
        .WillRepeatedly(testing::Return(false));
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
    enrollment_screen_.reset();
    mock_error_screen_.reset();
    network_portal_detector::Shutdown();
    network_handler_test_helper_.reset();
  }

 protected:
  // Mocks must outlive `enrollment_screen_`.
  NiceMock<MockEnrollmentScreenView> mock_view_;
  NiceMock<MockErrorScreenView> mock_error_view_;
  // Network portal must be initialized before destroying.
  std::unique_ptr<NiceMock<MockErrorScreen>> mock_error_screen_;
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;

  // Initializes NetworkHandler and required DBus clients.
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;

  // Closure passed to EnrollmentLauncher::SetupEnrollmentHelperMock
  // which creates the MockEnrollmentLauncher object that will
  // eventually be tied to the EnrollmentScreen. It also sets up the
  // appropriate expectations for testing with the Google Mock framework.
  // The template parameter should_enroll indicates whether or not
  // the EnrollmentLauncher should be mocked to successfully enroll.
  void SetupMockEnrollmentLauncher(AttestationEnrollmentStatus status) {
    if (status == AttestationEnrollmentStatus::SUCCESS) {
      // Define behavior of EnrollUsingAttestation to successfully enroll.
      EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([this]() {
            EnrollmentScreen* enrollment_screen =
                static_cast<EnrollmentScreen*>(
                    mock_enrollment_launcher_.status_consumer());
            EXPECT_EQ(enrollment_screen, enrollment_screen_.get());
            enrollment_screen->ShowEnrollmentStatusOnSuccess();
          }));
    } else {
      // Define behavior of EnrollUsingAttestation to fail to enroll.
      const policy::EnrollmentStatus enrollment_status =
          policy::EnrollmentStatus::ForRegistrationError(
              status == AttestationEnrollmentStatus::
                            DEVICE_NOT_SETUP_FOR_ZERO_TOUCH
                  ? policy::DeviceManagementStatus::
                        DM_STATUS_SERVICE_DEVICE_NOT_FOUND
                  : policy::DeviceManagementStatus::
                        DM_STATUS_TEMPORARY_UNAVAILABLE);
      EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([this, enrollment_status]() {
            EnrollmentScreen* enrollment_screen =
                static_cast<EnrollmentScreen*>(
                    mock_enrollment_launcher_.status_consumer());
            EXPECT_EQ(enrollment_screen, enrollment_screen_.get());
            enrollment_screen->OnEnrollmentError(enrollment_status);
          }));
    }
    // Define behavior of ClearAuth to only run the callback it is given.
    EXPECT_CALL(mock_enrollment_launcher_, ClearAuth(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(
            [](base::OnceClosure callback) { std::move(callback).Run(); }));
  }

  void ConfigureRestoreAfterRollback() {
    wizard_context_.configuration.Set(configuration::kRestoreAfterRollback,
                                      true);
  }

  void ShowEnrollmentScreen(bool suppress_jitter = false) {
    if (suppress_jitter) {
      // Remove jitter to enable deterministic testing.
      enrollment_screen_->retry_policy_.jitter_factor = 0;
    }
    enrollment_screen_->Show(&wizard_context_);
  }

  int GetEnrollmentScreenRetries() { return enrollment_screen_->num_retries_; }

  void TestEnrollmentFlowShouldComplete(
      const policy::EnrollmentConfig& config) {
    // Define behavior of MockEnrollmentLauncher to successfully enroll.
    SetupMockEnrollmentLauncher(AttestationEnrollmentStatus::SUCCESS);

    ScopedEnrollmentLauncherFactoryOverrideForTesting
        enrollment_launcher_factory_override(base::BindRepeating(
            FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

    SetUpEnrollmentScreen(config);

    ShowEnrollmentScreen();

    // Verify that enrollment flow finished and exited cleanly without
    // additional user input required.
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, last_screen_result_.value());
  }

 private:
  void HandleScreenExit(EnrollmentScreen::Result screen_result) {
    EXPECT_FALSE(last_screen_result_.has_value());
    last_screen_result_ = screen_result;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  ScopedStubInstallAttributes test_install_attributes_;

  TestingPrefServiceSimple pref_service_;

  system::FakeStatisticsProvider statistics_provider_;

  std::unique_ptr<EnrollmentScreen> enrollment_screen_;
  WizardContext wizard_context_;

  // The last result reported by `enrollment_screen_`.
  std::optional<EnrollmentScreen::Result> last_screen_result_;
};

TEST_F(EnrollmentScreenUnitTest, ConfigAfterRollback) {
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED;
  config.auth_mechanism =
      policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;

  // Expect that rollback enrollment config is passed to the view.
  EXPECT_CALL(
      mock_view_,
      SetEnrollmentConfig(testing::AllOf(
          testing::Field(
              &policy::EnrollmentConfig::mode,
              policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED),
          testing::Field(
              &policy::EnrollmentConfig::auth_mechanism,
              policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE))));

  EnrollmentScreenUnitTest::SetUpEnrollmentScreen(config);
}

TEST_F(EnrollmentScreenUnitTest, RollbackFlowShouldFinishEnrollmentScreen) {
  ConfigureRestoreAfterRollback();
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;
  config.auth_mechanism =
      policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
  TestEnrollmentFlowShouldComplete(config);
}

TEST_F(EnrollmentScreenUnitTest, RollbackFlowShouldNotRetryEnrollment) {
  ConfigureRestoreAfterRollback();
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;
  config.auth_mechanism =
      policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;

  // Define behavior of MockEnrollmentLauncher to always fail enrollment.
  SetupMockEnrollmentLauncher(AttestationEnrollmentStatus::DMSERVER_ERROR);

  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  SetUpEnrollmentScreen(config);

  ShowEnrollmentScreen(/*suppress_jitter=*/true);

  FastForwardTime(base::Days(1));

  EXPECT_EQ(GetEnrollmentScreenRetries(), 0);
}

}  // namespace ash
