// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/callback.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace crostini {

// TODO(okalitova): Install Ansible from backports repo once this is feasible.
extern const char kCrostiniDefaultAnsibleVersion[];

// AnsibleManagementService is responsible for Crostini default
// container management using Ansible.
class AnsibleManagementService : public KeyedService,
                                 public LinuxPackageOperationProgressObserver {
 public:
  // Observer class for Ansible Management Service related events.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;
    virtual void OnAnsibleSoftwareConfigurationStarted() = 0;
    virtual void OnAnsibleSoftwareConfigurationFinished(bool success) = 0;
  };

  static AnsibleManagementService* GetForProfile(Profile* profile);

  explicit AnsibleManagementService(Profile* profile);

  AnsibleManagementService(const AnsibleManagementService&) = delete;
  AnsibleManagementService& operator=(const AnsibleManagementService&) = delete;

  ~AnsibleManagementService() override;

  // |callback| is called once default Crostini container configuration is
  // finished.
  void ConfigureDefaultContainer(
      base::OnceCallback<void(bool success)> callback);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const ContainerId& container_id,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent,
                                     const std::string& error_message) override;
  void OnUninstallPackageProgress(const ContainerId& container_id,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

  void OnApplyAnsiblePlaybookProgress(
      vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal::Status status,
      const std::string& failure_details);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  void InstallAnsibleInDefaultContainer();
  void OnInstallAnsibleInDefaultContainer(CrostiniResult result);
  void GetAnsiblePlaybookToApply();
  void OnAnsiblePlaybookRetrieved(bool success);
  void ApplyAnsiblePlaybookToDefaultContainer();
  void OnApplyAnsiblePlaybook(
      absl::optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse>
          response);

  // Helper function that runs relevant callback and notifies observers.
  void OnConfigurationFinished(bool success);

  Profile* profile_;
  base::ObserverList<Observer> observers_;
  base::OnceCallback<void(bool success)> configuration_finished_callback_;
  std::string playbook_;
  base::WeakPtrFactory<AnsibleManagementService> weak_ptr_factory_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
