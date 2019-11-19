// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::test::DemoModeSetupResult;
using chromeos::test::SetupDummyOfflinePolicyDir;
using chromeos::test::SetupMockDemoModeNoEnrollmentHelper;
using chromeos::test::SetupMockDemoModeOfflineEnrollmentHelper;
using chromeos::test::SetupMockDemoModeOnlineEnrollmentHelper;
using testing::_;

namespace chromeos {

namespace {

class DemoSetupControllerTestHelper {
 public:
  DemoSetupControllerTestHelper()
      : run_loop_(std::make_unique<base::RunLoop>()) {}
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

  // Wait until the setup result arrives (either OnSetupError or OnSetupSuccess
  // is called), returns true when the result matches with |expected|.
  bool WaitResult(bool expected) {
    // Run() stops immediately if Quit is already called.
    run_loop_->Run();
    return succeeded_.has_value() && succeeded_.value() == expected;
  }

  // Returns true if powerwash is required to recover from the error.
  bool RequiresPowerwash() const {
    return error_.has_value() &&
           error_->recovery_method() ==
               DemoSetupController::DemoSetupError::RecoveryMethod::kPowerwash;
  }

  void Reset() {
    succeeded_.reset();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

 private:
  base::Optional<bool> succeeded_;
  base::Optional<DemoSetupController::DemoSetupError> error_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupControllerTestHelper);
};

}  // namespace

class DemoSetupControllerTest : public testing::Test {
 protected:
  DemoSetupControllerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}
  ~DemoSetupControllerTest() override = default;

  void SetUp() override {
    SystemSaltGetter::Initialize();
    DBusThreadManager::Initialize();
    SessionManagerClient::InitializeFake();
    DeviceSettingsService::Initialize();
    TestingBrowserProcess::GetGlobal()
        ->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetDeviceCloudPolicyManager()
        ->Initialize(TestingBrowserProcess::GetGlobal()->local_state());
    helper_ = std::make_unique<DemoSetupControllerTestHelper>();
    tested_controller_ = std::make_unique<DemoSetupController>();
  }

  void TearDown() override {
    EnterpriseEnrollmentHelper::SetEnrollmentHelperMock(nullptr);
    SessionManagerClient::Shutdown();
    DBusThreadManager::Shutdown();
    SystemSaltGetter::Shutdown();
    DeviceSettingsService::Shutdown();
  }

  static std::string GetDeviceRequisition() {
    return TestingBrowserProcess::GetGlobal()
        ->platform_part()
        ->browser_policy_connector_chromeos()
        ->GetDeviceCloudPolicyManager()
        ->GetDeviceRequisition();
  }

  std::unique_ptr<DemoSetupControllerTestHelper> helper_;
  std::unique_ptr<DemoSetupController> tested_controller_;

 private:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingLocalState testing_local_state_;
  ScopedStubInstallAttributes test_install_attributes_;
  system::ScopedFakeStatisticsProvider statistics_provider_;

  DISALLOW_COPY_AND_ASSIGN(DemoSetupControllerTest);
};

TEST_F(DemoSetupControllerTest, OfflineSuccess) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));
  SetupMockDemoModeOfflineEnrollmentHelper(DemoModeSetupResult::SUCCESS);
  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_))
      .WillOnce(testing::InvokeWithoutArgs(
          &mock_store, &policy::MockCloudPolicyStore::NotifyStoreLoaded));
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOffline);
  tested_controller_->SetPreinstalledOfflineResourcesPathForTesting(
      temp_dir.GetPath());
  tested_controller_->TryMountPreinstalledDemoResources(base::DoNothing());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(true));
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OfflineDeviceLocalAccountPolicyStoreFailed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));
  SetupMockDemoModeOfflineEnrollmentHelper(DemoModeSetupResult::SUCCESS);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_))
      .WillOnce(testing::InvokeWithoutArgs(
          &mock_store, &policy::MockCloudPolicyStore::NotifyStoreError));
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOffline);
  tested_controller_->SetPreinstalledOfflineResourcesPathForTesting(
      temp_dir.GetPath());
  tested_controller_->TryMountPreinstalledDemoResources(base::DoNothing());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_TRUE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OfflineInvalidDeviceLocalAccountPolicyBlob) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("", &temp_dir));
  SetupMockDemoModeOfflineEnrollmentHelper(DemoModeSetupResult::SUCCESS);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOffline);
  tested_controller_->SetPreinstalledOfflineResourcesPathForTesting(
      temp_dir.GetPath());
  tested_controller_->TryMountPreinstalledDemoResources(base::DoNothing());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_TRUE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OfflineErrorDefault) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));

  SetupMockDemoModeOfflineEnrollmentHelper(DemoModeSetupResult::ERROR_DEFAULT);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_)).Times(0);
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOffline);
  tested_controller_->SetPreinstalledOfflineResourcesPathForTesting(
      temp_dir.GetPath());
  tested_controller_->TryMountPreinstalledDemoResources(base::DoNothing());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OfflineErrorPowerwashRequired) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(SetupDummyOfflinePolicyDir("test", &temp_dir));

  SetupMockDemoModeOfflineEnrollmentHelper(
      DemoModeSetupResult::ERROR_POWERWASH_REQUIRED);

  policy::MockCloudPolicyStore mock_store;
  EXPECT_CALL(mock_store, Store(_)).Times(0);
  tested_controller_->SetDeviceLocalAccountPolicyStoreForTest(&mock_store);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOffline);
  tested_controller_->SetPreinstalledOfflineResourcesPathForTesting(
      temp_dir.GetPath());
  tested_controller_->TryMountPreinstalledDemoResources(base::DoNothing());
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_TRUE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OnlineSuccess) {
  SetupMockDemoModeOnlineEnrollmentHelper(DemoModeSetupResult::SUCCESS);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(true));
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OnlineErrorDefault) {
  SetupMockDemoModeOnlineEnrollmentHelper(DemoModeSetupResult::ERROR_DEFAULT);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OnlineErrorPowerwashRequired) {
  SetupMockDemoModeOnlineEnrollmentHelper(
      DemoModeSetupResult::ERROR_POWERWASH_REQUIRED);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_TRUE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, OnlineComponentError) {
  // Expect no enrollment attempt.
  SetupMockDemoModeNoEnrollmentHelper();

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_->SetCrOSComponentLoadErrorForTest(
      component_updater::CrOSComponentManager::Error::
          COMPATIBILITY_CHECK_FAILED);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());
}

TEST_F(DemoSetupControllerTest, EnrollTwice) {
  SetupMockDemoModeOnlineEnrollmentHelper(DemoModeSetupResult::ERROR_DEFAULT);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(false));
  EXPECT_FALSE(helper_->RequiresPowerwash());
  EXPECT_EQ("", GetDeviceRequisition());

  helper_->Reset();

  SetupMockDemoModeOnlineEnrollmentHelper(DemoModeSetupResult::SUCCESS);

  tested_controller_->set_demo_config(DemoSession::DemoModeConfig::kOnline);
  tested_controller_->Enroll(
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupSuccess,
                     base::Unretained(helper_.get())),
      base::BindOnce(&DemoSetupControllerTestHelper::OnSetupError,
                     base::Unretained(helper_.get())));

  EXPECT_TRUE(helper_->WaitResult(true));
  EXPECT_EQ("", GetDeviceRequisition());
}

}  //  namespace chromeos
