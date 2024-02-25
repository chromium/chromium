// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_TEST_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/crostini/ansible/mock_ansible_management_service.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"

class Profile;

namespace crostini {

// Helper class that provides common Ansible setup functionality for Crostini
// tests that involve Ansible management. Test helper assumes that
// DBusThreadManager is already initialized and Crostini is enabled.
class AnsibleManagementTestHelper {
 public:
  explicit AnsibleManagementTestHelper(Profile* profile);

  // Sets up and returns a mock instance of AnsibleManagementService.
  static MockAnsibleManagementService* SetUpMockAnsibleManagementService(
      Profile* profile);

  void SetUpAnsiblePlaybookPreference();
  void SetUpAnsibleInfra();

  void SetUpAnsibleInstallation(
      vm_tools::cicerone::InstallLinuxPackageResponse::Status status);
  void SetUpPlaybookApplication(
      vm_tools::cicerone::ApplyAnsiblePlaybookResponse::Status status);
  void SendSucceededInstallSignal();
  void SendSucceededApplySignal();
  void SendFailedInstallSignal();
  void SendFailedApplySignal();

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Owned by ash::DBusThreadManager
  raw_ptr<ash::FakeCiceroneClient, DanglingUntriaged> fake_cicerone_client_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_TEST_HELPER_H_
