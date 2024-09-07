// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/login/demo_mode/demo_mode_test_utils.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/login/enrollment/mock_enrollment_launcher.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_requisition_manager.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using test::DemoModeSetupResult;
using test::SetupDemoModeNoEnrollment;
using test::SetupDemoModeOnlineEnrollment;
using test::SetupDummyOfflinePolicyDir;
using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;

class DemoSetupControllerTestHelper {
 public:
  DemoSetupControllerTestHelper()
      : run_loop_(std::make_unique<base::RunLoop>()) {}

  DemoSetupControllerTestHelper(const DemoSetupControllerTestHelper&) = delete;
  DemoSetupControllerTestHelper& operator=(
      const DemoSetupControllerTestHelper&) = delete;

  virtual ~DemoSetupControllerTestHelper() = default;

  void OnSetupError(const DemoSetupController::DemoSetupError& error) {
    EXPECT_FALSE(succeeded_.has_value());
    succeeded_ = false;
    error_ = error;
    run_loop_->Quit();
  }

  void OnSetupSuccess() {
    EXPECT_FALSE(succeeded_.has_value());
    succeeded_ = true;
    run_loop_->Quit();
  }

  void SetCurrentSetupStep(DemoSetupController::DemoSetupStep current_step) {
    setup_step_ = current_step;
  }

  // Wait until the setup result arrives (either OnSetupError or OnSetupSuccess
  // is called), returns true when the success result matches with
  // `success_expected` and setup step matches `setup_step_expected`.
  bool WaitResult(bool success_expected,
                  DemoSetupController::DemoSetupStep setup_step_expected) {
    // Run() stops immediately if Quit is already called.
    run_loop_->Run();

    const bool success_check =
        succeeded_.has_value() && succeeded_.value() == success_expected;
    const bool setup_step_check =
        setup_step_.has_value() && setup_step_.value() == setup_step_expected;

    return success_check && setup_step_check;
  }

  // Returns true if powerwash is required to recover from the error.
  bool RequiresPowerwash() const {
    return error_.has_value() &&
           error_->recovery_method() ==
               DemoSetupController::DemoSetupError::RecoveryMethod::kPowerwash;
  }

  void Reset() {
    succeeded_.reset();
    setup_step_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  std::optional<bool> succeeded_;
  std::optional<DemoSetupController::DemoSetupStep> setup_step_;
  std::optional<DemoSetupController::DemoSetupError> error_;
  std::unique_ptr<base::RunLoop> run_loop_;
};

class DemoSetupControllerTest : public testing::Test {
 protected:
  DemoSetupControllerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  DemoSetupControllerTest(const DemoSetupControllerTest&) = delete;
  DemoSetupControllerTest& operator=(const DemoSetupControllerTest&) = delete;

  ~DemoSetupControllerTest() override = default;

  void SetUp() override {
    SystemSaltGetter::Initialize();
    DBusThreadManager::Initialize();
    SessionManagerClient::InitializeFake();
    DeviceSettingsService::Initialize();
    policy::EnrollmentRequisitionManager::Initialize();
  }

  void TearDown() override {
    SessionManagerClient::Shutdown();
    DBusThreadManager::Shutdown();
    SystemSaltGetter::Shutdown();
    DeviceSettingsService::Shutdown();
  }

  static std::string GetDeviceRequisition() {
    return policy::EnrollmentRequisitionManager::GetDeviceRequisition();
  }

  // Must be created first.
  base::test::TaskEnvironment task_environment_;

  // Mocks and helpers must outlive `tested_controller_`.
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher_;
  DemoSetupControllerTestHelper helper_;

  DemoSetupController tested_controller_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

 private:
  ScopedTestingLocalState testing_local_state_;
  ScopedStubInstallAttributes test_install_attributes_;
  system::ScopedFakeStatisticsProvider statistics_provider_;
};

TEST_F(DemoSetupControllerTest, OnlineSuccess) {
  SetupDemoModeOnlineEnrollment(&mock_enrollment_launcher_,
                                DemoModeSetupResult::SUCCESS);
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(
      helper_.WaitResult(true, DemoSetupController::DemoSetupStep::kComplete));
  EXPECT_EQ("", GetDeviceRequisition());

  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);

  // Both components were successfully loaded on the initial attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);
}

TEST_F(DemoSetupControllerTest, OnlineErrorDefault) {
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher;
  SetupDemoModeOnlineEnrollment(&mock_enrollment_launcher_,
                                DemoModeSetupResult::ERROR_DEFAULT);
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(helper_.WaitResult(
      false, DemoSetupController::DemoSetupStep::kEnrollment));
  EXPECT_FALSE(helper_.RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());

  // SetupDemoModeOnlineEnrollment() with DemoModeSetupResult::ERROR_DEFAULT
  // maps to policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE,
  // which matches to
  // DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable in
  // DemoSetupController::CreateFromClientStatus().
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);

  // The error occurred at the enrollment step. In the previous component
  // loading step, both components were still successfully loaded on the initial
  // attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);
}

TEST_F(DemoSetupControllerTest, OnlineErrorPowerwashRequired) {
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher;
  SetupDemoModeOnlineEnrollment(&mock_enrollment_launcher_,
                                DemoModeSetupResult::ERROR_POWERWASH_REQUIRED);
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(helper_.WaitResult(
      false, DemoSetupController::DemoSetupStep::kEnrollment));
  EXPECT_TRUE(helper_.RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());

  // SetupDemoModeOnlineEnrollment() with
  // DemoModeSetupResult::ERROR_POWERWASH_REQUIRED maps to
  // policy::DeviceManagementStatus::LOCK_ALREADY_LOCKED, which matches to
  // DemoSetupController::DemoSetupError::ErrorCode::kAlreadyLocked in
  // DemoSetupController::CreateFromClientStatus().
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kAlreadyLocked, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);

  // The error occurred at the enrollment step. In the previous component
  // loading step, both components were still successfully loaded on the initial
  // attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);
}

TEST_F(DemoSetupControllerTest, OnlineComponentError) {
  // Expect no enrollment attempt.
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher;
  SetupDemoModeNoEnrollment(&mock_enrollment_launcher_);
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.SetCrOSComponentLoadErrorForTest(
      component_updater::ComponentManagerAsh::Error::
          COMPATIBILITY_CHECK_FAILED);
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(helper_.WaitResult(
      false, DemoSetupController::DemoSetupStep::kEnrollment));
  EXPECT_FALSE(helper_.RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());

  // SetCrOSComponentLoadErrorForTest() will lead to
  // DemoSetupController::DemoSetupError::ErrorCode::kOnlineComponentError.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kOnlineComponentError, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);
}

TEST_F(DemoSetupControllerTest, EnrollTwice) {
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher;
  SetupDemoModeOnlineEnrollment(&mock_enrollment_launcher_,
                                DemoModeSetupResult::ERROR_DEFAULT);
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(helper_.WaitResult(
      false, DemoSetupController::DemoSetupStep::kEnrollment));
  EXPECT_FALSE(helper_.RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());

  // SetupDemoModeOnlineEnrollment() with DemoModeSetupResult::ERROR_DEFAULT
  // maps to policy::DeviceManagementStatus::DM_STATUS_TEMPORARY_UNAVAILABLE,
  // which matches to
  // DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable in
  // DemoSetupController::CreateFromClientStatus().
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 1);

  // The error occurred at the enrollment step. In the previous component
  // loading step, both components were still successfully loaded on the initial
  // attempt.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 0);

  helper_.Reset();
  Mock::VerifyAndClearExpectations(&mock_enrollment_launcher_);

  SetupDemoModeOnlineEnrollment(&mock_enrollment_launcher_,
                                DemoModeSetupResult::SUCCESS);

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(
      helper_.WaitResult(true, DemoSetupController::DemoSetupStep::kComplete));
  EXPECT_EQ("", GetDeviceRequisition());

  // The enum of success (no error) is recorded to DemoMode.Setup.Error on
  // success. There should have been two counts because of two tries.
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kTemporaryUnavailable, 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.Error",
      DemoSetupController::DemoSetupError::ErrorCode::kSuccess, 1);
  histogram_tester_.ExpectTotalCount("DemoMode.Setup.Error", 2);

  // On retry, both components were successfully loaded again regardless that
  // they were successfully loaded before.
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentInitialLoadingResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentInitialLoadingResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
  histogram_tester_.ExpectTotalCount(
      "DemoMode.Setup.ComponentLoadingRetryResult", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Setup.ComponentLoadingRetryResult",
      DemoSetupController::DemoSetupComponentLoadingResult::
          kAppSuccessResourcesSuccess,
      1);
}

TEST_F(DemoSetupControllerTest, GetSubOrganizationEmail) {
  std::string email = DemoSetupController::GetSubOrganizationEmail();

  // kDemoModeCountry defaults to "US".
  EXPECT_EQ(email, "admin-us@cros-demo-mode.com");

  // Test other supported countries.
  const std::string testing_supported_countries[] = {
      "US", "AT", "AU", "BE", "BR", "CA", "DE", "DK", "ES",
      "FI", "FR", "GB", "IE", "IN", "IT", "JP", "LU", "MX",
      "NL", "NO", "NZ", "PL", "PT", "SE", "ZA"};

  for (auto country : testing_supported_countries) {
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                country);
    email = DemoSetupController::GetSubOrganizationEmail();

    std::string country_lowercase = base::ToLowerASCII(country);
    EXPECT_EQ(email,
              "admin-" + country_lowercase + "@" + policy::kDemoModeDomain);
  }

  // Test unsupported country string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry, "KR");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");

  // Test unsupported region string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                              "NORDIC");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");

  // Test random string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry, "foo");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");
}

TEST_F(DemoSetupControllerTest, GetSubOrganizationEmailWithLowercase) {
  std::string email = DemoSetupController::GetSubOrganizationEmail();

  // kDemoModeCountry defaults to "US".
  EXPECT_EQ(email, "admin-us@cros-demo-mode.com");

  // Test other supported countries.
  const std::string testing_supported_countries[] = {
      "us", "be", "ca", "dk", "fi", "fr", "de", "ie",
      "it", "jp", "lu", "nl", "no", "es", "se", "gb"};

  for (auto country : testing_supported_countries) {
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                country);
    email = DemoSetupController::GetSubOrganizationEmail();

    EXPECT_EQ(email, "admin-" + country + "@" + policy::kDemoModeDomain);
  }

  // Test unsupported country string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry, "kr");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");
}

TEST_F(DemoSetupControllerTest, GetSubOrganizationEmailForBlazeyDevice) {
  feature_list_.InitAndEnableFeature(chromeos::features::kCloudGamingDevice);

  std::string email;

  // Test other supported countries.
  const std::string testing_supported_countries[] = {
      "US", "AT", "AU", "BE", "BR", "CA", "DE", "DK", "ES",
      "FI", "FR", "GB", "IE", "IN", "IT", "JP", "LU", "MX",
      "NL", "NO", "NZ", "PL", "PT", "SE", "ZA"};

  for (auto country : testing_supported_countries) {
    g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                                country);
    email = DemoSetupController::GetSubOrganizationEmail();

    std::string country_lowercase = base::ToLowerASCII(country);
    EXPECT_EQ(email, "admin-" + country_lowercase + "-blazey@" +
                         policy::kDemoModeDomain);
  }

  // Test unsupported country string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry, "KR");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");

  // Test unsupported region string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry,
                                              "NORDIC");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");

  // Test random string.
  g_browser_process->local_state()->SetString(prefs::kDemoModeCountry, "foo");
  email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "");
}

TEST_F(DemoSetupControllerTest, GetSubOrganizationEmailForCustomOU) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kDemoModeEnrollingUsername, "test-user-name");

  std::string email = DemoSetupController::GetSubOrganizationEmail();
  EXPECT_EQ(email, "test-user-name@cros-demo-mode.com");
}

TEST_F(DemoSetupControllerTest, OnlineSuccessWithValidRetailerAndStore) {
  NiceMock<MockEnrollmentLauncher> mock_enrollment_launcher;
  SetupDemoModeOnlineEnrollment(&mock_enrollment_launcher_,
                                DemoModeSetupResult::SUCCESS);
  ScopedEnrollmentLauncherFactoryOverrideForTesting
      enrollment_launcher_factory_override(base::BindRepeating(
          FakeEnrollmentLauncher::Create, &mock_enrollment_launcher_));

  tested_controller_.set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_.SetAndCanonicalizeRetailerName("Retailer");
  tested_controller_.set_store_number("1234");
  tested_controller_.Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(&helper_)),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(&helper_)),
      base::BindRepeating(&DemoSetupControllerTestHelper::SetCurrentSetupStep,
                          base::Unretained(&helper_)));

  EXPECT_TRUE(
      helper_.WaitResult(true, DemoSetupController::DemoSetupStep::kComplete));
  EXPECT_EQ("", GetDeviceRequisition());
  EXPECT_EQ("retailer", g_browser_process->local_state()->GetString(
                            prefs::kDemoModeRetailerId));
  EXPECT_EQ("1234", g_browser_process->local_state()->GetString(
                        prefs::kDemoModeStoreId));
}

struct RetailerNameCanonicalizationTestCase {
  std::string retailer_name;
  std::string canonicalized_retailer_name;
};

class RetailerNameCanonicalizationTest
    : public DemoSetupControllerTest,
      public ::testing::WithParamInterface<
          RetailerNameCanonicalizationTestCase> {
 public:
  RetailerNameCanonicalizationTest() = default;
  ~RetailerNameCanonicalizationTest() override = default;
};

TEST_P(RetailerNameCanonicalizationTest, SetAndCanonicalizeRetailerName) {
  tested_controller_.SetAndCanonicalizeRetailerName(GetParam().retailer_name);
  ASSERT_EQ(tested_controller_.get_retailer_name_for_testing(),
            GetParam().canonicalized_retailer_name);
}

const RetailerNameCanonicalizationTestCase kRetailerNameTestCases[] = {
    {"retailer", "retailer"},
    {"RETAILER", "retailer"},
    {"ReTaiLeR", "retailer"},
    {"retailer with spaces", "retailerwithspaces"},
    {"retailer' w:th $ymbols", "retailerwthymbols"},
    // Don't remove numeric chars
    {"r3ta1ler", "r3ta1ler"},
    // Test various case-sensitive diacritics and non-latin characters
    {"rétailër", "rétailër"},
    {"RÉTAILËR", "rétailër"},
    {"RÆTÅILØR", "rætåilør"},
    {"بائع تجزئة", "بائعتجزئة"},
    {"小売業者.com", "小売業者com"}};

INSTANTIATE_TEST_SUITE_P(TestRetailerNameTransformations,
                         RetailerNameCanonicalizationTest,
                         testing::ValuesIn(kRetailerNameTestCases));

}  // namespace
}  //  namespace ash
