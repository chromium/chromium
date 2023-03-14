// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/ansible/ansible_management_test_helper.h"

#include "base/files/file_util.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/prefs/pref_service.h"

namespace crostini {

// static
MockAnsibleManagementService*
AnsibleManagementTestHelper::SetUpMockAnsibleManagementService(
    Profile* profile) {
  return static_cast<MockAnsibleManagementService*>(
      AnsibleManagementServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile, base::BindRepeating(
                       [](Profile* profile, content::BrowserContext* context) {
                         return MockAnsibleManagementService::Build(profile);
                       },
                       profile)));
}

AnsibleManagementTestHelper::AnsibleManagementTestHelper(Profile* profile)
    : profile_(profile) {
  fake_cicerone_client_ = ash::FakeCiceroneClient::Get();
}

void AnsibleManagementTestHelper::SetUpAnsiblePlaybookPreference() {
  base::FilePath ansible_playbook_file_path =
      profile_->GetPath().AppendASCII("playbook.yaml");
  const char playbook[] = "---";
  base::WriteFile(ansible_playbook_file_path, playbook);
  profile_->GetPrefs()->SetFilePath(prefs::kCrostiniAnsiblePlaybookFilePath,
                                    ansible_playbook_file_path);
}

void AnsibleManagementTestHelper::SetUpAnsibleInfra() {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndEnableFeature(
      features::kCrostiniAnsibleInfrastructure);

  SetUpAnsiblePlaybookPreference();
}

void AnsibleManagementTestHelper::SetUpAnsibleInstallation(
    vm_tools::cicerone::InstallLinuxPackageResponse::Status status) {
  vm_tools::cicerone::InstallLinuxPackageResponse response;
  response.set_status(status);
  fake_cicerone_client_->set_install_linux_package_response(response);
}

void AnsibleManagementTestHelper::SetUpPlaybookApplication(
    vm_tools::cicerone::ApplyAnsiblePlaybookResponse::Status status) {
  vm_tools::cicerone::ApplyAnsiblePlaybookResponse response;
  response.set_status(status);
  fake_cicerone_client_->set_apply_ansible_playbook_response(response);
}

void AnsibleManagementTestHelper::SendSucceededInstallSignal() {
  vm_tools::cicerone::InstallLinuxPackageProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile_));
  signal.set_vm_name(kCrostiniDefaultVmName);
  signal.set_container_name(kCrostiniDefaultContainerName);
  signal.set_status(
      vm_tools::cicerone::InstallLinuxPackageProgressSignal::SUCCEEDED);

  fake_cicerone_client_->InstallLinuxPackageProgress(signal);
}

void AnsibleManagementTestHelper::SendSucceededApplySignal() {
  vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile_));
  signal.set_vm_name(kCrostiniDefaultVmName);
  signal.set_container_name(kCrostiniDefaultContainerName);
  signal.set_status(
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::SUCCEEDED);

  fake_cicerone_client_->NotifyApplyAnsiblePlaybookProgress(signal);
}

void AnsibleManagementTestHelper::SendFailedInstallSignal() {
  vm_tools::cicerone::InstallLinuxPackageProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile_));
  signal.set_vm_name(kCrostiniDefaultVmName);
  signal.set_container_name(kCrostiniDefaultContainerName);
  signal.set_status(
      vm_tools::cicerone::InstallLinuxPackageProgressSignal::FAILED);
  signal.set_failure_details("banana");

  fake_cicerone_client_->InstallLinuxPackageProgress(signal);
}

void AnsibleManagementTestHelper::SendFailedApplySignal() {
  vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal signal;
  signal.set_owner_id(CryptohomeIdForProfile(profile_));
  signal.set_vm_name(kCrostiniDefaultVmName);
  signal.set_container_name(kCrostiniDefaultContainerName);
  signal.set_status(
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::FAILED);
  signal.set_failure_details("apple");

  fake_cicerone_client_->NotifyApplyAnsiblePlaybookProgress(signal);
}
}  // namespace crostini
