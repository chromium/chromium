// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper.h"
#include "chrome/browser/ash/login/enrollment/enterprise_enrollment_helper_mock.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

namespace {

policy::EnrollmentConfig GetZeroTouchEnrollmentConfig() {
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED;
  config.auth_mechanism = policy::EnrollmentConfig::AUTH_MECHANISM_ATTESTATION;
  return config;
}

policy::EnrollmentConfig GetZeroTouchEnrollmentConfigForFallback() {
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION;
  config.auth_mechanism =
      policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
  return config;
}

void ConfigureZeroTouchEnrollment() {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kEnterpriseEnableZeroTouchEnrollment, "hands-off");
}

}  // namespace

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Invoke;

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
        mock_view_.AsWeakPtr(),
        base::BindRepeating(&EnrollmentScreenUnitTest::HandleScreenExit,
                            base::Unretained(this)));

    enrollment_screen_->SetEnrollmentConfig(config);
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  MockEnrollmentScreenView* GetMockScreenView() { return &mock_view_; }

  // testing::Test:
  void SetUp() override {
    RegisterLocalState(pref_service_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&pref_service_);
    system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    policy::EnrollmentRequisitionManager::Initialize();
  }

  void TearDown() override {
    TestingBrowserProcess::GetGlobal()->SetShuttingDown(true);
    TestingBrowserProcess::GetGlobal()->SetLocalState(nullptr);
  }

 protected:
  // Objects required by the EnrollmentScreen that can be re-used.
  MockEnrollmentScreenView mock_view_;

  // Closure passed to EnterpriseEnrollmentHelper::SetupEnrollmentHelperMock
  // which creates the EnterpriseEnrollmentHelperMock object that will
  // eventually be tied to the EnrollmentScreen. It also sets up the
  // appropriate expectations for testing with the Google Mock framework.
  // The template parameter should_enroll indicates whether or not
  // the EnterpriseEnrollmentHelper should be mocked to successfully enroll.
  void SetupMockEnrollmentHelper(AttestationEnrollmentStatus status) {
    std::unique_ptr<EnterpriseEnrollmentHelperMock> mock =
        std::make_unique<EnterpriseEnrollmentHelperMock>();
    EnterpriseEnrollmentHelperMock* mock_ptr = mock.get();
    if (status == AttestationEnrollmentStatus::SUCCESS) {
      // Define behavior of EnrollUsingAttestation to successfully enroll.
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([mock_ptr]() {
            static_cast<EnrollmentScreen*>(mock_ptr->status_consumer())
                ->ShowEnrollmentStatusOnSuccess();
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
      EXPECT_CALL(*mock, EnrollUsingAttestation())
          .Times(AnyNumber())
          .WillRepeatedly(Invoke([mock_ptr, enrollment_status]() {
            mock_ptr->status_consumer()->OnEnrollmentError(enrollment_status);
          }));
    }
    // Define behavior of ClearAuth to only run the callback it is given.
    EXPECT_CALL(*mock, ClearAuth(_))
        .Times(AnyNumber())
        .WillRepeatedly(Invoke(
            [](base::OnceClosure callback) { std::move(callback).Run(); }));

    EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(std::move(mock));
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

  void ScheduleUserRetry(base::TimeDelta delay) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&EnrollmentScreen::OnRetry,
                       enrollment_screen_->weak_ptr_factory_.GetWeakPtr()),
        delay);
  }

  int GetEnrollmentScreenRetries() { return enrollment_screen_->num_retries_; }

  void TestEnrollmentFlowShouldComplete(
      const policy::EnrollmentConfig& config) {
    // Define behavior of EnterpriseEnrollmentHelperMock to successfully enroll.
    SetupMockEnrollmentHelper(AttestationEnrollmentStatus::SUCCESS);

    SetUpEnrollmentScreen(config);

    ShowEnrollmentScreen();

    // Verify that enrollment flow finished and exited cleanly without
    // additional user input required.
    ASSERT_TRUE(last_screen_result_.has_value());
    EXPECT_EQ(EnrollmentScreen::Result::COMPLETED, last_screen_result_.value());
  }

  void TestEnrollmentFlowRetriesOnFailure(
      const policy::EnrollmentConfig& config) {
    // Define behavior of EnterpriseEnrollmentHelperMock to always fail
    // enrollment.
    SetupMockEnrollmentHelper(AttestationEnrollmentStatus::DMSERVER_ERROR);

    SetUpEnrollmentScreen(config);

    ShowEnrollmentScreen(/*suppress_jitter=*/true);

    // Fast forward time by 1 minute.
    FastForwardTime(base::Minutes(1));

    // Check that we have retried 4 times.
    EXPECT_EQ(GetEnrollmentScreenRetries(), 4);
  }

  void TestEnrollmentFlowShouldUseFallback(
      const policy::EnrollmentConfig& config) {
    // Define behavior of EnterpriseEnrollmentHelperMock to fail
    // attestation-based enrollment.
    SetupMockEnrollmentHelper(
        AttestationEnrollmentStatus::DEVICE_NOT_SETUP_FOR_ZERO_TOUCH);

    SetUpEnrollmentScreen(config);

    // Once we fallback we show a sign in screen for manual enrollment.
    EXPECT_CALL(*GetMockScreenView(), Show()).Times(2);

    // Start enrollment.
    ShowEnrollmentScreen();
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
  absl::optional<EnrollmentScreen::Result> last_screen_result_;
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

TEST_F(EnrollmentScreenUnitTest, RollbackFlowShouldRetryEnrollment) {
  ConfigureRestoreAfterRollback();
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT;
  config.auth_mechanism =
      policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
  TestEnrollmentFlowRetriesOnFailure(config);
}

TEST_F(EnrollmentScreenUnitTest, ZeroTouchFlowShouldFinishEnrollmentScreen) {
  ConfigureZeroTouchEnrollment();

  TestEnrollmentFlowShouldComplete(GetZeroTouchEnrollmentConfig());
}

TEST_F(EnrollmentScreenUnitTest,
       ZeroTouchFlowShouldFallbackToManualEnrollment) {
  ConfigureZeroTouchEnrollment();

  TestEnrollmentFlowShouldUseFallback(
      GetZeroTouchEnrollmentConfigForFallback());
}

TEST_F(EnrollmentScreenUnitTest, ZeroTouchFlowShouldRetryEnrollment) {
  ConfigureZeroTouchEnrollment();

  TestEnrollmentFlowRetriesOnFailure(GetZeroTouchEnrollmentConfig());
}

TEST_F(EnrollmentScreenUnitTest, ZeroTouchFlowShouldNotRetryOnTopOfUser) {
  ConfigureZeroTouchEnrollment();

  // Define behavior of EnterpriseEnrollmentHelperMock to always fail
  // enrollment.
  SetupMockEnrollmentHelper(AttestationEnrollmentStatus::DMSERVER_ERROR);

  SetUpEnrollmentScreen(GetZeroTouchEnrollmentConfig());

  // Start zero-touch enrollment.
  ShowEnrollmentScreen(/*suppress_jitter=*/true);

  // Schedule user retry button click after 30 sec.
  ScheduleUserRetry(base::Seconds(30));

  // Fast forward time by 1 minute.
  FastForwardTime(base::Minutes(1));

  // Check that the number of retries is still 4.
  EXPECT_EQ(GetEnrollmentScreenRetries(), 4);
}

TEST_F(EnrollmentScreenUnitTest, ZeroTouchFlowShouldNotRetryAfterSuccess) {
  ConfigureZeroTouchEnrollment();

  // Define behavior of EnterpriseEnrollmentHelperMock to successfully enroll.
  SetupMockEnrollmentHelper(AttestationEnrollmentStatus::SUCCESS);

  SetUpEnrollmentScreen(GetZeroTouchEnrollmentConfig());

  // Start zero-touch enrollment.
  ShowEnrollmentScreen();

  // Fast forward time by 1 minute.
  FastForwardTime(base::Minutes(1));

  // Check that we do not retry.
  EXPECT_EQ(GetEnrollmentScreenRetries(), 0);
}

class AutomaticEnrollmentScreenUnitTest
    : public EnrollmentScreenUnitTest,
      public ::testing::WithParamInterface<policy::EnrollmentConfig::Mode> {
 public:
  AutomaticEnrollmentScreenUnitTest() = default;

  AutomaticEnrollmentScreenUnitTest(const AutomaticEnrollmentScreenUnitTest&) =
      delete;
  AutomaticEnrollmentScreenUnitTest& operator=(
      const AutomaticEnrollmentScreenUnitTest&) = delete;

  void SetUp() override {
    EnrollmentScreenUnitTest::SetUp();

    // Configure the browser to use Hands-Off Enrollment. This is required here
    // to test for proper completion of the enrollment process.
    ConfigureZeroTouchEnrollment();
  }

  policy::EnrollmentConfig GetEnrollmentConfig() {
    policy::EnrollmentConfig config;
    config.mode = GetParam();
    config.auth_mechanism =
        policy::EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE;
    return config;
  }
};

TEST_P(AutomaticEnrollmentScreenUnitTest, ShowErrorPanel) {
  // We use Zero-Touch's test for retries as a way to know that there was
  // an error pane with a Retry button displayed to the user when we encounter
  // a DMServer error that is not that the device isn't setup for Auto RE.
  TestEnrollmentFlowRetriesOnFailure(GetEnrollmentConfig());
}

TEST_P(AutomaticEnrollmentScreenUnitTest, FinishEnrollmentFlow) {
  TestEnrollmentFlowShouldComplete(GetEnrollmentConfig());
}

TEST_P(AutomaticEnrollmentScreenUnitTest, Fallback) {
  TestEnrollmentFlowShouldUseFallback(GetEnrollmentConfig());
}

INSTANTIATE_TEST_SUITE_P(
    P,
    AutomaticEnrollmentScreenUnitTest,
    ::testing::Values(
        policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED));

}  // namespace ash
