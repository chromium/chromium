// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_management_service.h"

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
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

using base::test::TestFuture;

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
        AnsibleManagementServiceFactory::GetForProfile(profile_.get());
    test_helper_ =
        std::make_unique<AnsibleManagementTestHelper>(profile_.get());
    test_helper_->SetUpAnsibleInfra();

    SetUpViewsEnvironmentForTesting();
  }

  AnsibleManagementServiceTest(const AnsibleManagementServiceTest&) = delete;
  AnsibleManagementServiceTest& operator=(const AnsibleManagementServiceTest&) =
      delete;

  ~AnsibleManagementServiceTest() override {
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
    is_install_ansible_success_ = true;
    is_apply_ansible_success_ = true;
    ansible_management_service_->AddObserver(this);
  }

  void TearDown() override {
    ansible_management_service_->RemoveObserver(this);
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
  raw_ptr<CrostiniManager, DanglingUntriaged> crostini_manager_;
  raw_ptr<AnsibleManagementService, DanglingUntriaged>
      ansible_management_service_;
  bool is_install_ansible_success_;
  bool is_apply_ansible_success_;

 protected:
  AnsibleManagementService* ansible_management_service() {
    return ansible_management_service_;
  }

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

  TestFuture<bool> result_future;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerInstallFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  TestFuture<bool> result_future;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerInstallSignalFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  SetInstallAnsibleStatus(false);

  TestFuture<bool> result_future;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerApplyFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::FAILED);

  TestFuture<bool> result_future;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(AnsibleManagementServiceTest, ConfigureContainerApplySignalFail) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);
  SetApplyAnsibleStatus(false);

  TestFuture<bool> result_future;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());
}

TEST_F(AnsibleManagementServiceTest,
       CouldNotConfigureContainerAfterSuccessfullConfiguration) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);

  TestFuture<bool> result_future;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_TRUE(result_future.Get());

  TestFuture<bool> result_future2;
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future2.GetCallback());
  EXPECT_FALSE(result_future2.Get());
}

TEST_F(AnsibleManagementServiceTest,
       CouldConfigureContainerAfterFailedConfiguration) {
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::FAILED);

  TestFuture<bool> result_future;
  // Unsuccessful sequence of events.
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future.GetCallback());
  EXPECT_FALSE(result_future.Get());

  TestFuture<bool> result_future2;
  test_helper_->SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::STARTED);
  test_helper_->SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::STARTED);
  ansible_management_service()->ConfigureContainer(
      DefaultContainerId(),
      profile_->GetPrefs()->GetFilePath(
          prefs::kCrostiniAnsiblePlaybookFilePath),
      result_future2.GetCallback());
  EXPECT_TRUE(result_future2.Get());
}

}  // namespace crostini
