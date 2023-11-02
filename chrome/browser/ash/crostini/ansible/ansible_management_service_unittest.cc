// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"

#include "base/test/bind.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_util.h"
#include "chrome/browser/ui/views/crostini/crostini_ansible_software_config_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
void ExpectResult(base::OnceClosure closure,
                  bool expected_result,
                  bool actual_result) {
  EXPECT_EQ(expected_result, actual_result);
  std::move(closure).Run();
}
}  // namespace

namespace crostini {

class AnsibleManagementServiceTest : public testing::Test,
                                     public AnsibleManagementService::Observer {
 public:
  AnsibleManagementServiceTest() {
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();

    profile_ = std::make_unique<TestingProfile>();
    crostini_manager_ = CrostiniManager::GetForProfile(profile_.get());
    ansible_management_service_ =
        AnsibleManagementService::GetForProfile(profile_.get());
    test_helper_ =
        std::make_unique<AnsibleManagementTestHelper>(profile_.get());
    test_helper_->SetUpAnsibleInfra();

    SetUpViewsEnvironmentForTesting();
  }

  AnsibleManagementServiceTest(const AnsibleManagementServiceTest&) = delete;
  AnsibleManagementServiceTest& operator=(const AnsibleManagementServiceTest&) =
      delete;

  ~AnsibleManagementServiceTest() override {
    base::RunLoop().RunUntilIdle();

    TearDownViewsEnvironmentForTesting();

    test_helper_.reset();
    ansible_management_service_->Shutdown();
    crostini_manager_->Shutdown();
    profile_.reset();

    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  void SetUp() override {
    run_loop_ = std::make_unique<base::RunLoop>();
    is_install_ansible_success_ = true;
    is_apply_ansible_success_ = true;
    ansible_management_service_->AddObserver(this);
  }

  void TearDown() override {
    run_loop_.reset();
    ansible_management_service_->RemoveObserver(this);
  }

  void ExpectTrueResult(bool success) {
    EXPECT_TRUE(success);
    run_loop()->Quit();
  }

  void ExpectFalseResult(bool success) {
    EXPECT_FALSE(success);
    run_loop()->Quit();
  }

  CrostiniAnsibleSoftwareConfigView* ActiveView(
      const guest_os::GuestId& container_id) {
    if (ansible_management_service_->GetDialogWidgetForTesting(container_id)) {
      return (CrostiniAnsibleSoftwareConfigView*)ansible_management_service_
          ->GetDialogWidgetForTesting(container_id)
          ->widget_delegate();
    } else {
      return nullptr;
    }
  }

  // AnsibleManagementService::Observer
  void OnAnsibleSoftwareConfigurationStarted(
      const guest_os::GuestId& container_id) override {}
  void OnAnsibleSoftwareConfigurationFinished(
      const guest_os::GuestId& container_id,
      bool success) override {}
  void OnAnsibleSoftwareConfigurationUiPrompt(
      const guest_os::GuestId& container_id,
      bool interactive) override {
    if (interactive) {
      // Press retry/ok on dialog if it's waiting for input.
      ActiveView(container_id)->Accept();
    }
  }
  void OnAnsibleSoftwareInstall(
      const guest_os::GuestId& container_id) override {
    if (is_install_ansible_success_) {
      test_helper_->SendSucceededInstallSignal();
    } else {
      test_helper_->SendFailedInstallSignal();
    }
  }
  void OnApplyAnsiblePlaybook(const guest_os::GuestId& container_id) override {
    if (is_apply_ansible_success_) {
      test_helper_->SendSucceededApplySignal();
    } else {
      test_helper_->SendFailedApplySignal();
    }
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  CrostiniManager* crostini_manager_;
  AnsibleManagementService* ansible_management_service_;
  bool is_install_ansible_success_;
  bool is_apply_ansible_success_;

 protected:
  AnsibleManagementService* ansible_management_service() {
    return ansible_management_service_;
  }

  base::RunLoop* run_loop() { return run_loop_.get(); }

  void SetInstallAnsibleStatus(bool status) {
    is_install_ansible_success_ = status;
  }
  void SetApplyAnsibleStatus(bool status) {
    is_apply_ansible_success_ = status;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<AnsibleManagementTestHelper> test_helper_;
  std::unique_ptr<TestingProfile> profile_;

  base::WeakPtrFactory<AnsibleManagementServiceTest> weak_ptr_factory_{this};
};

TEST_F(AnsibleManagementServiceTest, ConfigureContainerSuccess) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);

  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&AnsibleManagementServiceTest::ExpectTrueResult,
                     weak_ptr_factory_.GetWeakPtr()));
  run_loop()->Run();
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerInstallFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&AnsibleManagementServiceTest::ExpectFalseResult,
                     weak_ptr_factory_.GetWeakPtr()));
  run_loop()->Run();
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerInstallSignalFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  SetInstallAnsibleStatus(false);

  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&AnsibleManagementServiceTest::ExpectFalseResult,
                     weak_ptr_factory_.GetWeakPtr()));
  run_loop()->Run();
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerApplyFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED);

  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&AnsibleManagementServiceTest::ExpectFalseResult,
                     weak_ptr_factory_.GetWeakPtr()));
  run_loop()->Run();
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerApplySignalFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);
  SetApplyAnsibleStatus(false);

  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&AnsibleManagementServiceTest::ExpectFalseResult,
                     weak_ptr_factory_.GetWeakPtr()));
  run_loop()->Run();
}

TEST_F(AnsibleManagementServiceTest,
       CouldNotConfigureContainerAfterSuccessfullConfiguration) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);

  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&ExpectResult, base::BindLambdaForTesting([&]() {
        ansible_management_service()->ConfigureContainer(
            DefaultContainerId(),
            profile_->GetPrefs()->GetFilePath(
                prefs::kCrostiniAnsiblePlaybookFilePath),
            base::BindOnce(&AnsibleManagementServiceTest::ExpectFalseResult,
                           weak_ptr_factory_.GetWeakPtr()));
      }),
                     true));
  run_loop()->Run();
}

TEST_F(AnsibleManagementServiceTest,
       CouldConfigureContainerAfterFailedConfiguration) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  // Unsuccessful sequence of events.
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      base::BindOnce(&ExpectResult, base::BindLambdaForTesting([&]() {
        // Setup for success.
        test_helper_->SetUpAnsibleInstallation(
            vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
        test_helper_->SetUpPlaybookApplication(
            vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);
        ansible_management_service()->ConfigureContainer(
            DefaultContainerId(),
            profile_->GetPrefs()->GetFilePath(
                prefs::kCrostiniAnsiblePlaybookFilePath),
            base::BindOnce(&AnsibleManagementServiceTest::ExpectTrueResult,
                           weak_ptr_factory_.GetWeakPtr()));
      }),
                     false));
  run_loop()->Run();
}

}  // namespace crostini
