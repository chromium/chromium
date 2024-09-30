// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/enrollment/enrollment_screen.h"

#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include "ash/constants/ash_features.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_screen.h"
#include "chrome/browser/ash/login/enrollment/mock_oauth2_token_revoker.h"
#include "chrome/browser/ash/login/enrollment/oauth2_token_revoker.h"
#include "chrome/browser/ash/login/enrollment/timebound_user_context_holder.h"
#include "chrome/browser/ash/login/screens/account_selection_screen.h"
#include "chrome/browser/ash/login/screens/mock_error_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/portal_detector/mock_network_portal_detector.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

constexpr char kTestDomain[] = "test.org";
constexpr char kTestAuthCode[] = "test_auth_code";
constexpr char kTestDeviceId[] = "test_device_id";
constexpr char kTestUserEmail[] = "user@test.org";
constexpr char kTestUserGaiaId[] = "test_user_gaia_id";
constexpr char kTestUserPassword[] = "test_password";
constexpr char kTestRefreshToken[] = "test_refresh_token";

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;

namespace {

// Initialize network-related objects used by `EnrollmentScreen` and
// `ErrorScreen`.
class ScopedNetworkInitializer {
 public:
  ScopedNetworkInitializer() {
    network_handler_test_helper_.AddDefaultProfiles();
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
  NetworkHandlerTestHelper network_handler_test_helper_;
};

}  // namespace

class EnrollmentScreenBaseTest : public testing::Test {
 public:
  EnrollmentScreenBaseTest(const EnrollmentScreenBaseTest&) = delete;
  EnrollmentScreenBaseTest& operator=(const EnrollmentScreenBaseTest&) = delete;

 protected:
  EnrollmentScreenBaseTest()
      : mock_error_screen_(mock_error_view_.AsWeakPtr()) {
    RegisterLocalState(fake_local_state_.registry());
    TestingBrowserProcess::GetGlobal()->SetLocalState(&fake_local_state_);

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
    enrollment_screen_->set_tpm_updater_for_testing(base::DoNothing());
  }

  // Fast forwards time by the specified amount.
  void FastForwardTime(base::TimeDelta time) {
    task_environment_.FastForwardBy(time);
  }

  void ExpectAttestationBasedEnrollmentAndReportEnrolled() {
    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
        .WillOnce([this]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          SetupEnrolledDevice();
          enrollment_screen_->OnDeviceEnrolled();
        });

    ExpectSetEnterpriseDomainInfo();
  }

  void ExpectAttestationBasedEnrollmentAndReportFailure(
      policy::EnrollmentStatus status) {
    EXPECT_NE(status.enrollment_code(),
              policy::EnrollmentStatus::Code::kSuccess)
        << "Cannot not expect failure with a success code";

    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAttestation())
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

  void ExpectManualEnrollmentAndReportEnrolled() {
    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAuthCode(kTestAuthCode))
        .WillOnce([this]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          SetupEnrolledDevice();
          enrollment_screen_->OnDeviceEnrolled();
        });

    ExpectSetEnterpriseDomainInfo();
  }

  void ExpectManualEnrollmentAndReportFailure(policy::EnrollmentStatus status) {
    EXPECT_NE(status.enrollment_code(),
              policy::EnrollmentStatus::Code::kSuccess)
        << "Cannot not expect failure with a success code";

    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingAuthCode(kTestAuthCode))
        .WillOnce([this, status]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          enrollment_screen_->OnEnrollmentError(status);
        });
  }

  void ExpectManualEnrollmentAndReportFailure() {
    ExpectManualEnrollmentAndReportFailure(
        policy::EnrollmentStatus::ForRegistrationError(
            policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  }

  void ExpectGetDeviceAttributeUpdatePermission(bool permission_granted) {
    EXPECT_CALL(mock_enrollment_launcher_, GetDeviceAttributeUpdatePermission())
        .WillOnce([this, permission_granted]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          enrollment_screen_->OnDeviceAttributeUpdatePermission(
              permission_granted);
        });
  }

  void ExpectSuccessScreen() {
    EXPECT_CALL(mock_view_, ShowEnrollmentStatus(
                                policy::EnrollmentStatus::ForEnrollmentCode(
                                    policy::EnrollmentStatus::Code::kSuccess)))
        .WillOnce([this]() { enrollment_screen_->OnConfirmationClosed(); });
  }

  void ExpectErrorScreen(policy::EnrollmentStatus status) {
    EXPECT_NE(status.enrollment_code(),
              policy::EnrollmentStatus::Code::kSuccess)
        << "Cannot not expect failure with a success code";

    EXPECT_CALL(mock_view_, ShowEnrollmentStatus(status));
  }

  void ExpectTokenBasedEnrollmentAndReportEnrolled() {
    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingEnrollmentToken())
        .WillOnce([this]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          SetupEnrolledDevice();
          enrollment_screen_->OnDeviceEnrolled();
        });
  }

  void ExpectTokenBasedEnrollmentAndReportFailure() {
    ExpectTokenBasedEnrollmentAndReportFailure(
        policy::EnrollmentStatus::EnrollmentStatus::ForRegistrationError(
            policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  }

  void ExpectTokenBasedEnrollmentAndReportFailure(
      policy::EnrollmentStatus status) {
    EXPECT_NE(status.enrollment_code(),
              policy::EnrollmentStatus::Code::kSuccess)
        << "Cannot not expect failure with a success code";

    EXPECT_CALL(mock_enrollment_launcher_, EnrollUsingEnrollmentToken())
        .WillOnce([this, status]() {
          ExpectEnrollmentScreenIsEnrollmentStatusConsumer();
          enrollment_screen_->OnEnrollmentError(status);
        });
  }

  void ExpectErrorScreen() {
    ExpectErrorScreen(policy::EnrollmentStatus::ForRegistrationError(
        policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE));
  }

  void ExpectSetEnterpriseDomainInfo() {
    EXPECT_CALL(mock_view_, SetEnterpriseDomainInfo(kTestDomain, _));
  }

  void ExpectClearAuth(bool expect_oauth2_tokens_revoked = true) {
    EXPECT_CALL(mock_enrollment_launcher_,
                ClearAuth(_, expect_oauth2_tokens_revoked))
        .Times(AnyNumber())
        .WillRepeatedly(
            [](base::OnceClosure callback, bool revoke_oauth2_tokens) {
              std::move(callback).Run();
            });
  }

  void ExpectEnrollmentConfig(policy::EnrollmentConfig::Mode mode) {
    EXPECT_CALL(mock_view_, SetEnrollmentConfig(testing::AllOf(testing::Field(
                                &policy::EnrollmentConfig::mode, mode))));
  }

  void ExpectEnrollmentConfig(policy::EnrollmentConfig::Mode mode,
                              std::string enrollment_token) {
    EXPECT_CALL(mock_view_,
                SetEnrollmentConfig(testing::AllOf(
                    testing::Field(&policy::EnrollmentConfig::mode, mode),
                    testing::Field(&policy::EnrollmentConfig::enrollment_token,
                                   enrollment_token))));
  }

  void ExpectShowView() { EXPECT_CALL(mock_view_, Show()); }

  void ExpectShowViewWithLogin(
      policy::LicenseType license_type = policy::LicenseType::kEnterprise) {
    EXPECT_CALL(mock_view_, Show()).WillOnce([this, license_type]() {
      login::OnlineSigninArtifacts signin_artifacts;
      signin_artifacts.email = kTestUserEmail;
      signin_artifacts.gaia_id = kTestUserGaiaId;
      signin_artifacts.password = kTestUserPassword;
      signin_artifacts.using_saml = false;

      enrollment_screen_->OnLoginDone(std::move(signin_artifacts),
                                      static_cast<int>(license_type),
                                      kTestAuthCode);
    });
  }

  void ShowEnrollmentScreen(bool suppress_jitter = false) {
    if (suppress_jitter) {
      // Remove jitter to enable deterministic testing.
      enrollment_screen_->retry_policy_.jitter_factor = 0;
    }
    enrollment_screen_->Show(&wizard_context());

    FlushTasksAndWaitCompletion();
  }

  void UserCancel() {
    enrollment_screen_->OnCancel();
    FlushTasksAndWaitCompletion();
  }

  void UserRetry() {
    enrollment_screen_->OnRetry();
    FlushTasksAndWaitCompletion();
  }

  int GetEnrollmentScreenRetries() const {
    return enrollment_screen_->num_retries_;
  }

  const auto& last_screen_result() const { return last_screen_result_; }

  WizardContext& wizard_context() {
    return CHECK_DEREF(fake_login_display_host_.GetWizardContext());
  }

  TestingPrefServiceSimple& local_state() { return fake_local_state_; }

  MockEnrollmentLauncher& mock_enrollment_launcher() {
    return mock_enrollment_launcher_;
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

  void SetupEnrolledDevice() {
    test_install_attributes_.Get()->SetCloudManaged(kTestDomain, kTestDeviceId);
    test_install_attributes_.Get()->set_device_locked(true);
  }

  void FlushTasksAndWaitCompletion() {
    // Trigger and wait for async tasks to finish. E.g.
    // `StartupUtils::MarkDeviceRegistered`.
    FastForwardTime(base::TimeDelta());
    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Must outlive `mock_error_screen_`.
  ScopedNetworkInitializer scoped_network_;

  // Mocks must outlive `enrollment_screen_`.
  NiceMock<MockEnrollmentScreenView> mock_view_;
  NiceMock<MockErrorScreenView> mock_error_view_;
  NiceMock<MockErrorScreen> mock_error_screen_;
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      scoped_enrollment_launcher_factory_override_{
          base::BindRepeating(FakeEnrollmentLauncher::Create,
                              &mock_enrollment_launcher_)};

  // Used by `enrollment_screen_`.
  ScopedStubInstallAttributes test_install_attributes_;

  // Used by `EnrollmentRequisitionManager` and `StartupUtils`.
  TestingPrefServiceSimple fake_local_state_;

  // Used by `EnrollmentRequisitionManager`.
  system::ScopedFakeStatisticsProvider fake_statistics_provider_;

  // Used by `enrollment_screen_`.
  FakeLoginDisplayHost fake_login_display_host_;

  std::unique_ptr<EnrollmentScreen> enrollment_screen_;

  // The last result reported by `enrollment_screen_`.
  std::optional<EnrollmentScreen::Result> last_screen_result_;
};

class EnrollmentScreenManualFlowTest
    : public EnrollmentScreenBaseTest,
      public ::testing::WithParamInterface<
          std::tuple<policy::EnrollmentConfig::Mode, bool>> {
 public:
  EnrollmentScreenManualFlowTest() {
    if (ShouldEnableOobeAddUserDuringEnrollment()) {
      feature_list_.InitAndEnableFeature(
          features::kOobeAddUserDuringEnrollment);
    }
  }

  static std::string ParamInfoToString(
      const testing::TestParamInfo<EnrollmentScreenManualFlowTest::ParamType>&
          info) {
    const std::string feature_enabled = std::get<1>(info.param)
                                            ? "WithAddUserAfterEnrollment"
                                            : "WithoutAddUserAfterEnrollment";
    const policy::EnrollmentConfig::Mode mode = std::get<0>(info.param);
    return base::ToString(mode) + "_" + feature_enabled;
  }

 protected:
  policy::EnrollmentConfig::Mode GetParamEnrollmentMode() {
    return std::get<0>(GetParam());
  }

  bool ShouldEnableOobeAddUserDuringEnrollment() {
    return std::get<1>(GetParam());
  }

  policy::EnrollmentConfig GetEnrollmentConfig() {
    policy::EnrollmentConfig config;
    config.mode = GetParamEnrollmentMode();
    DCHECK(!config.is_mode_attestation())
        << "Config must not be attestation: " << config;

    return config;
  }

  // If the feature which adds a user from cached credentials is enabled,
  // depending on the enrollment mode the tokens might be saved or not.
  // Only in case of the manual enrollment the tokens will be saved to be reused
  // later on, thus they should not be revoked.
  void SetupClearAuthExpectationsOnSuccess() {
    const policy::EnrollmentConfig::Mode enrollment_mode =
        GetParamEnrollmentMode();
    const bool expect_oauth2_tokens_revoked =
        !(enrollment_mode == policy::EnrollmentConfig::MODE_MANUAL &&
          features::IsOobeAddUserDuringEnrollmentEnabled());
    ExpectClearAuth(expect_oauth2_tokens_revoked);
  }

  base::test::ScopedFeatureList feature_list_;
};

TEST_P(EnrollmentScreenManualFlowTest, ShouldFinishEnrollmentScreen) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode);

  ExpectShowViewWithLogin();
  ExpectManualEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  ExpectSuccessScreen();
  SetupClearAuthExpectationsOnSuccess();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
  EXPECT_EQ(local_state().GetInteger(prefs::kDeviceRegistered), 1);
}

TEST_P(EnrollmentScreenManualFlowTest, ShouldNotAutomaticallyRetryEnrollment) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode);

  ExpectShowViewWithLogin();
  ExpectManualEnrollmentAndReportFailure();
  ExpectErrorScreen();
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  FastForwardTime(base::Days(1));

  EXPECT_EQ(GetEnrollmentScreenRetries(), 0);
  EXPECT_FALSE(last_screen_result().has_value());
}

TEST_P(EnrollmentScreenManualFlowTest, ShouldRetryEnrollmentOnUserAction) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(config.mode);
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportFailure();
    ExpectErrorScreen();
    ExpectClearAuth();

    // Second view is shown after user retry.
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportEnrolled();
    ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
    ExpectSuccessScreen();
    SetupClearAuthExpectationsOnSuccess();
  }

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());

  UserRetry();

  EXPECT_EQ(GetEnrollmentScreenRetries(), 1);
  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
  EXPECT_EQ(local_state().GetInteger(prefs::kDeviceRegistered), 1);
}

INSTANTIATE_TEST_SUITE_P(
    ManualEnrollment,
    EnrollmentScreenManualFlowTest,
    ::testing::Combine(
        ::testing::Values(policy::EnrollmentConfig::MODE_MANUAL,
                          policy::EnrollmentConfig::MODE_MANUAL_REENROLLMENT,
                          policy::EnrollmentConfig::MODE_LOCAL_FORCED,
                          policy::EnrollmentConfig::MODE_LOCAL_ADVERTISED,
                          policy::EnrollmentConfig::MODE_SERVER_FORCED,
                          policy::EnrollmentConfig::MODE_SERVER_ADVERTISED,
                          policy::EnrollmentConfig::MODE_RECOVERY,
                          policy::EnrollmentConfig::MODE_INITIAL_SERVER_FORCED),
        /*IsOobeAddUserDuringEnrollmentEnabled=*/::testing::Bool()),
    EnrollmentScreenManualFlowTest::ParamInfoToString);

// Signin artifacts and the refresh token can be optionally preserved in the
// wizard context to be used later on, outside the EnrollmentScreen, in order to
// skip the second sign in screen.
class EnrollmentAddUserTest : public EnrollmentScreenBaseTest {
 public:
  EnrollmentAddUserTest() {
    feature_list_.InitAndEnableFeature(features::kOobeAddUserDuringEnrollment);
  }

 protected:
  class DummyTokenRevoker : public OAuth2TokenRevokerBase {
    void Start(const std::string& token) override {
      LOG(WARNING) << "Revoking a token: " << token;
    }
  };

  void ExpectGetRefreshToken() {
    EXPECT_CALL(mock_enrollment_launcher(), GetOAuth2RefreshToken)
        .WillOnce(::testing::Return(kTestRefreshToken));
  }

  policy::EnrollmentConfig GetManualConfig() {
    policy::EnrollmentConfig config;
    config.mode = policy::EnrollmentConfig::MODE_MANUAL;
    return config;
  }

  void ExpectCredentialsCached() {
    EXPECT_TRUE(wizard_context().timebound_user_context_holder);
    const TimeboundUserContextHolder* const user_context_holder =
        wizard_context().timebound_user_context_holder.get();
    EXPECT_TRUE(user_context_holder->HasUserContext());
    EXPECT_EQ(user_context_holder->GetAccountId().GetUserEmail(),
              kTestUserEmail);
    EXPECT_EQ(user_context_holder->GetGaiaID(), kTestUserGaiaId);
    EXPECT_TRUE(user_context_holder->GetPassword());
    EXPECT_EQ(user_context_holder->GetPassword().value(),
              PasswordInput(kTestUserPassword));
    EXPECT_EQ(user_context_holder->GetRefreshToken(), kTestRefreshToken);
  }

  void ExpectCredentialsNotCached() {
    EXPECT_FALSE(wizard_context().timebound_user_context_holder);
  }

  // Timebound user context will revoke the tokens on destruction. To prevent a
  // crash in testing we need to replace the used token revoker with a stub one.
  void SetupFakeTokenRevoker() {
    wizard_context()
        .timebound_user_context_holder->InjectTokenRevokerForTesting(
            std::make_unique<DummyTokenRevoker>());
  }

  // TimeboundUserContext will revoke the tokens either on timeout or
  // destruction.
  void ExpectTokensRevokedByTimeboundUserContext() {
    std::unique_ptr<MockOAuth2TokenRevoker> mock_token_revoker =
        std::make_unique<MockOAuth2TokenRevoker>();
    EXPECT_CALL(*mock_token_revoker, Start(_)).Times(testing::Exactly(1));
    wizard_context()
        .timebound_user_context_holder->InjectTokenRevokerForTesting(
            std::move(mock_token_revoker));
  }

  base::test::ScopedFeatureList feature_list_;
};

// This test makes sure that data is properly stored, and that
// the token is not revoked when enrollment is successful.
TEST_F(EnrollmentAddUserTest,
       ShouldSaveUserContextAndNotRevokeTokensOnSuccess) {
  policy::EnrollmentConfig config = GetManualConfig();

  ExpectShowViewWithLogin();
  ExpectManualEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  ExpectSuccessScreen();
  ExpectGetRefreshToken();
  ExpectClearAuth(/*expect_oauth2_tokens_revoked=*/false);

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  ExpectCredentialsCached();
  SetupFakeTokenRevoker();
}

// Make sure that the data is not stored and the tokens are revoked in case
// the enrollment failed.
TEST_F(EnrollmentAddUserTest,
       ShouldNotSaveUserContextAndShouldRevokeTokensOnEnrollmentFailed) {
  policy::EnrollmentConfig config = GetManualConfig();

  ExpectShowViewWithLogin();
  ExpectManualEnrollmentAndReportFailure();
  ExpectErrorScreen();
  ExpectClearAuth(/*expect_oauth2_tokens_revoked=*/true);

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  ExpectCredentialsNotCached();
}

// Make sure that the data is not stored and the tokens are revoked in case
// the enrollment is canceled.
TEST_F(EnrollmentAddUserTest,
       ShouldNotSaveUserContextAndShouldRevokeTokensOnEnrollmentCanceled) {
  policy::EnrollmentConfig config = GetManualConfig();

  ExpectShowViewWithLogin();
  ExpectClearAuth(/*expect_oauth2_tokens_revoked=*/true);

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  UserCancel();

  ExpectCredentialsNotCached();
}

TEST_F(EnrollmentAddUserTest,
       ShouldRevokeTokenAndClearUserContextAfterTimeout) {
  policy::EnrollmentConfig config = GetManualConfig();

  ExpectShowViewWithLogin();
  ExpectManualEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  ExpectSuccessScreen();
  ExpectGetRefreshToken();
  ExpectClearAuth(/*expect_oauth2_tokens_revoked=*/false);

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  ExpectCredentialsCached();

  // Wait until the credentials expire.
  ExpectTokensRevokedByTimeboundUserContext();
  FastForwardTime(TimeboundUserContextHolder::kCredentialsVlidityPeriod);
  ExpectCredentialsNotCached();
}

class EnrollmentScreenAttestationFlowTest
    : public EnrollmentScreenBaseTest,
      public ::testing::WithParamInterface<policy::EnrollmentConfig::Mode> {
 protected:
  EnrollmentScreenAttestationFlowTest() {
    if (IsRollbackFlow()) {
      wizard_context().configuration.Set(configuration::kRestoreAfterRollback,
                                         true);
    }
  }
  policy::EnrollmentConfig GetEnrollmentConfig() {
    policy::EnrollmentConfig config;
    config.mode = GetParam();
    DCHECK(config.is_mode_attestation())
        << "Config must be attestation: " << config;

    return config;
  }
  bool IsRollbackFlow() const {
    return GetParam() ==
           policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED;
  }
};

TEST_P(EnrollmentScreenAttestationFlowTest, ShouldFinishEnrollmentScreen) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode);

  ExpectAttestationBasedEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  if (!IsRollbackFlow()) {
    ExpectSuccessScreen();
  }
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
  EXPECT_EQ(local_state().GetInteger(prefs::kDeviceRegistered), 1);
}

TEST_P(EnrollmentScreenAttestationFlowTest,
       ShouldNotAutomaticallyRetryEnrollment) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode);
  ExpectAttestationBasedEnrollmentAndReportFailure();
  ExpectErrorScreen();
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen(/*suppress_jitter=*/true);

  FastForwardTime(base::Days(1));

  EXPECT_EQ(GetEnrollmentScreenRetries(), 0);
  EXPECT_FALSE(last_screen_result().has_value());
}

TEST_P(EnrollmentScreenAttestationFlowTest, ShouldRetryEnrollmentOnUserAction) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(config.mode);
    ExpectShowView();
    ExpectAttestationBasedEnrollmentAndReportFailure();
    ExpectErrorScreen();

    // Second view is shown after user retry.
    ExpectShowView();
    ExpectAttestationBasedEnrollmentAndReportEnrolled();
    ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
    if (!IsRollbackFlow()) {
      ExpectSuccessScreen();
    }
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());

  UserRetry();

  EXPECT_EQ(GetEnrollmentScreenRetries(), 1);
  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
  EXPECT_EQ(local_state().GetInteger(prefs::kDeviceRegistered), 1);
}

// The add user flow is expected to only affect the manual enrollment.
// Whenever the enrollment is not manual the tokens should be revoked
// and no data should be saved wihin the wizard context.
TEST_P(EnrollmentScreenAttestationFlowTest,
       ShouldNotPreserveUserContextAndShouldRevokeTokens) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kOobeAddUserDuringEnrollment);

  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode);

  ExpectAttestationBasedEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  if (!IsRollbackFlow()) {
    ExpectSuccessScreen();
  }
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  UserContext* user_context = wizard_context().user_context.get();
  EXPECT_FALSE(user_context);
}

INSTANTIATE_TEST_SUITE_P(
    AttestationBasedEnrollment,
    EnrollmentScreenAttestationFlowTest,
    ::testing::Values(
        policy::EnrollmentConfig::MODE_ATTESTATION,
        policy::EnrollmentConfig::MODE_ATTESTATION_LOCAL_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED));

class EnrollmentScreenAttestationFlowWithManualFallbackTest
    : public EnrollmentScreenAttestationFlowTest {
 protected:
  policy::EnrollmentConfig GetEnrollmentConfigForManualFallback() {
    return GetEnrollmentConfig().GetManualFallbackConfig();
  }
};

TEST_P(EnrollmentScreenAttestationFlowWithManualFallbackTest,
       ShouldAutomaticallyFallbackToManuallEnrollment) {
  const policy::EnrollmentConfig initial_config = GetEnrollmentConfig();
  const policy::EnrollmentConfig fallback_config =
      GetEnrollmentConfigForManualFallback();
  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(initial_config.mode);
    ExpectShowView();
    ExpectAttestationBasedEnrollmentAndReportFailureWithAutomaticFallback();

    // Second view is shown for manual fallback.
    ExpectEnrollmentConfig(fallback_config.mode);
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportEnrolled();
    ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
    if (!IsRollbackFlow()) {
      ExpectSuccessScreen();
    }
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen(initial_config);
  ShowEnrollmentScreen();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
  EXPECT_EQ(local_state().GetInteger(prefs::kDeviceRegistered), 1);
}

TEST_P(EnrollmentScreenAttestationFlowWithManualFallbackTest,
       ShouldFallbackToManualEnrollmentOnUserAction) {
  const policy::EnrollmentConfig initial_config = GetEnrollmentConfig();
  const policy::EnrollmentConfig fallback_config =
      GetEnrollmentConfigForManualFallback();
  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(initial_config.mode);
    ExpectShowView();
    ExpectAttestationBasedEnrollmentAndReportFailure();
    ExpectErrorScreen();

    // Second view is shown for manual fallback. This should be triggered after
    // user decides to fallback.
    ExpectEnrollmentConfig(fallback_config.mode);
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportEnrolled();
    ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
    if (!IsRollbackFlow()) {
      ExpectSuccessScreen();
    }
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen(initial_config);
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());

  UserCancel();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
  EXPECT_EQ(local_state().GetInteger(prefs::kDeviceRegistered), 1);
}

INSTANTIATE_TEST_SUITE_P(
    ManualFallbackEnrollment,
    EnrollmentScreenAttestationFlowWithManualFallbackTest,
    ::testing::Values(
        policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED,
        policy::EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED));

class EnrollmentScreenTokenBasedEnrollmentTest
    : public EnrollmentScreenBaseTest {
 protected:
  EnrollmentScreenTokenBasedEnrollmentTest() = default;

  policy::EnrollmentConfig GetEnrollmentConfig() {
    policy::EnrollmentConfig config;
    config.mode =
        policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
    // The token isn't used directly by EnrollmentScreen, but let's set it here
    // for realism.
    config.enrollment_token = policy::test::kEnrollmentToken;
    return config;
  }

  policy::EnrollmentConfig GetEnrollmentConfigForManualFallback() {
    return GetEnrollmentConfig().GetManualFallbackConfig();
  }

  system::ScopedFakeStatisticsProvider statistics_provider_;
  base::test::ScopedCommandLine command_line_;
  policy::test::EnrollmentTestHelper enrollment_test_helper_{
      &command_line_, &statistics_provider_};
};

TEST_F(EnrollmentScreenTokenBasedEnrollmentTest, ShouldFinishEnrollmentScreen) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode, config.enrollment_token);

  ExpectTokenBasedEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(false);
  ExpectSuccessScreen();
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
}

// Enrollment tokens are currently only retrieved from OOBE config if the device
// is chrome-branded, so we need to have this preprocessor check in order to run
// this test.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(EnrollmentScreenTokenBasedEnrollmentTest,
       EnrollmentTokenConfigIsDeletedAfterEnrollmentSuccess) {
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  const std::string* present_enrollment_token =
      enrollment_test_helper_.GetEnrollmentTokenFromOobeConfiguration();
  ASSERT_EQ(*present_enrollment_token, policy::test::kEnrollmentToken);

  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode, config.enrollment_token);

  ExpectTokenBasedEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(false);
  ExpectSuccessScreen();
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  const std::string* missing_flex_token =
      enrollment_test_helper_.GetEnrollmentTokenFromOobeConfiguration();
  EXPECT_EQ(missing_flex_token, nullptr);
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

TEST_F(EnrollmentScreenTokenBasedEnrollmentTest,
       ShouldRetryEnrollmentOnUserAction) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  {
    testing::InSequence s;
    // First view is shown for token-based enrollment failure.
    ExpectEnrollmentConfig(config.mode, config.enrollment_token);
    ExpectShowView();
    ExpectTokenBasedEnrollmentAndReportFailure();
    ExpectErrorScreen();

    // Second view is shown after user retry.
    ExpectShowView();
    ExpectTokenBasedEnrollmentAndReportEnrolled();
    ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
    ExpectSuccessScreen();
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());

  UserRetry();

  EXPECT_EQ(GetEnrollmentScreenRetries(), 1);
  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
}

TEST_F(EnrollmentScreenTokenBasedEnrollmentTest,
       ShouldNotAutomaticallyRetryEnrollment) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();

  ExpectEnrollmentConfig(config.mode);
  ExpectTokenBasedEnrollmentAndReportFailure();
  ExpectErrorScreen();
  ExpectClearAuth();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen(/*suppress_jitter=*/true);

  FastForwardTime(base::Days(1));

  EXPECT_EQ(GetEnrollmentScreenRetries(), 0);
  EXPECT_FALSE(last_screen_result().has_value());
}

// Unlike with attestation, DEVICE_NOT_FOUND doesn't really make sense for
// enrollment, so we shouldn't automatically fall back to manual if we get
// this status (although we don't expect to). We should perhaps automatically
// fall back if the server encounters an invalid or not present token, but that
// status hasn't yet been enumerated.
//
// TODO(b/329271128): Change this test once the proper DeviceManagementStatus
// for TOKEN_NOT_FOUND is added and handled in the server response.
TEST_F(EnrollmentScreenTokenBasedEnrollmentTest,
       ShouldNotAutomaticallyFallbackToManualEnrollment) {
  const policy::EnrollmentConfig config = GetEnrollmentConfig();
  {
    testing::InSequence s;
    // First view is shown for attestation-based failure.
    ExpectEnrollmentConfig(config.mode, config.enrollment_token);
    ExpectShowView();
    ExpectTokenBasedEnrollmentAndReportFailure(
        policy::EnrollmentStatus::ForRegistrationError(
            policy::DeviceManagementStatus::
                DM_STATUS_SERVICE_DEVICE_NOT_FOUND));

    // Verify that we land at the error screen instead of the GAIA sign-in
    // screen.
    ExpectErrorScreen(policy::EnrollmentStatus::ForRegistrationError(
        policy::DeviceManagementStatus::DM_STATUS_SERVICE_DEVICE_NOT_FOUND));
  }

  ExpectClearAuth();
  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());
}

TEST_F(EnrollmentScreenTokenBasedEnrollmentTest,
       ShouldFallbackToManualEnrollmentOnUserAction) {
  const policy::EnrollmentConfig initial_config = GetEnrollmentConfig();
  const policy::EnrollmentConfig fallback_config =
      GetEnrollmentConfigForManualFallback();
  {
    testing::InSequence s;
    // First view is shown for token-based failure.
    ExpectEnrollmentConfig(initial_config.mode,
                           initial_config.enrollment_token);
    ExpectShowView();
    ExpectTokenBasedEnrollmentAndReportFailure();
    ExpectErrorScreen();

    // Second view is shown for manual fallback. This should be triggered after
    // user decides to fallback.
    ExpectEnrollmentConfig(fallback_config.mode,
                           fallback_config.enrollment_token);
    ExpectShowViewWithLogin();
    ExpectManualEnrollmentAndReportEnrolled();
    ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
    ExpectSuccessScreen();
  }

  ExpectClearAuth();

  SetUpEnrollmentScreen(initial_config);
  ShowEnrollmentScreen();

  EXPECT_FALSE(last_screen_result().has_value());

  UserCancel();

  EXPECT_EQ(last_screen_result(), EnrollmentScreen::Result::COMPLETED);
}

class EnrollmentScreenLicenseTest : public EnrollmentScreenBaseTest {
 protected:
  void ExpectLicense(policy::LicenseType license_type) {
    EXPECT_CALL(
        mock_enrollment_launcher(),
        Setup(testing::AllOf(testing::Field(
                  &policy::EnrollmentConfig::license_type, license_type)),
              _));
  }
};

// Test that user can override prescribed license type on manual enrollment.
TEST_F(EnrollmentScreenLicenseTest,
       UserCanChooseLicenseTypeOnManualEnrollment) {
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_MANUAL;
  config.license_type = policy::LicenseType::kEducation;

  ExpectShowViewWithLogin(policy::LicenseType::kTerminal);
  ExpectLicense(policy::LicenseType::kTerminal);
  ExpectManualEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  ExpectSuccessScreen();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();
}

TEST_F(EnrollmentScreenLicenseTest,
       AttestationBasedEnrollmentPicksPrescribedLicense) {
  policy::EnrollmentConfig config;
  config.mode = policy::EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED;
  config.license_type = policy::LicenseType::kTerminal;

  ExpectLicense(policy::LicenseType::kTerminal);
  ExpectAttestationBasedEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  ExpectSuccessScreen();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();
}

TEST_F(EnrollmentScreenLicenseTest, TokenEnrollmentPicksPrescribedLicense) {
  policy::EnrollmentConfig config;
  config.mode =
      policy::EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED;
  config.enrollment_token = policy::test::kEnrollmentToken;
  config.license_type = policy::LicenseType::kEnterprise;

  ExpectLicense(policy::LicenseType::kEnterprise);
  ExpectTokenBasedEnrollmentAndReportEnrolled();
  ExpectGetDeviceAttributeUpdatePermission(/*permission_granted=*/false);
  ExpectSuccessScreen();

  SetUpEnrollmentScreen(config);
  ShowEnrollmentScreen();
}

}  // namespace ash
