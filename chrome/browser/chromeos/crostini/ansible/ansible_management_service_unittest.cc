// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"

#include "base/test/mock_callback.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class AnsibleManagementServiceTest : public testing::Test {
 public:
  AnsibleManagementServiceTest() {
    chromeos::DBusThreadManager::Initialize();
    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = CrostiniManager::GetForProfile(profile_.get());
    ansible_management_service_ =
        AnsibleManagementService::GetForProfile(profile_.get());
    test_helper_ =
        std::make_unique<AnsibleManagementTestHelper>(profile_.get());
    test_helper_->SetUpAnsibleInfra();

    SetUpViewsEnvironmentForTesting();
  }

  ~AnsibleManagementServiceTest() override {
    crostini::CloseCrostiniAnsibleSoftwareConfigViewForTesting();
    // Wait for view triggered to be closed.
    base::RunLoop().RunUntilIdle();
    TearDownViewsEnvironmentForTesting();

    test_helper_.reset();
    ansible_management_service_->Shutdown();
    crostini_manager_->Shutdown();
    profile_.reset();
    chromeos::DBusThreadManager::Shutdown();
  }

 protected:
  AnsibleManagementService* ansible_management_service() {
    return ansible_management_service_;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AnsibleManagementTestHelper> test_helper_;
  base::MockCallback<base::OnceCallback<void(bool)>>
      configuration_finished_mock_callback_;

 private:
  std::unique_ptr<TestingProfile> profile_;
  CrostiniManager* crostini_manager_;
  AnsibleManagementService* ansible_management_service_;

  DISALLOW_COPY_AND_ASSIGN(AnsibleManagementServiceTest);
};

TEST_F(AnsibleManagementServiceTest, ConfigureDefaultContainerSuccess) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);

  EXPECT_CALL(configuration_finished_mock_callback_, Run(true)).Times(1);

  ansible_management_service()->ConfigureDefaultContainer(
      configuration_finished_mock_callback_.Get());
  base::RunLoop().RunUntilIdle();

  test_helper_->SendSucceededInstallSignal();
  base::RunLoop().RunUntilIdle();
  task_environment_.RunUntilIdle();

  test_helper_->SendSucceededApplySignal();
}

TEST_F(AnsibleManagementServiceTest, ConfigureDefaultContainerInstallFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  EXPECT_CALL(configuration_finished_mock_callback_, Run(false)).Times(1);

  ansible_management_service()->ConfigureDefaultContainer(
      configuration_finished_mock_callback_.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(AnsibleManagementServiceTest, ConfigureDefaultContainerApplyFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED);

  EXPECT_CALL(configuration_finished_mock_callback_, Run(false)).Times(1);

  ansible_management_service()->ConfigureDefaultContainer(
      configuration_finished_mock_callback_.Get());
  base::RunLoop().RunUntilIdle();

  test_helper_->SendSucceededInstallSignal();
  base::RunLoop().RunUntilIdle();
  task_environment_.RunUntilIdle();
}

}  // namespace crostini
