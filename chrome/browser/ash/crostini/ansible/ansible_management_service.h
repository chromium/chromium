// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
#define CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace views {
class Widget;
}  // namespace views

namespace crostini {

struct AnsibleConfiguration {
  AnsibleConfiguration(std::string playbook,
                       base::FilePath path,
                       base::OnceCallback<void(bool success)> callback);
  AnsibleConfiguration(base::FilePath path,
                       base::OnceCallback<void(bool success)> callback);
  ~AnsibleConfiguration();

  std::string playbook;
  base::FilePath path;
  base::OnceCallback<void(bool success)> callback;
};

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
    virtual void OnAnsibleSoftwareConfigurationStarted(
        const guest_os::GuestId& container_id) = 0;
    virtual void OnAnsibleSoftwareConfigurationProgress(
        const guest_os::GuestId& container_id,
        const std::vector<std::string>& status_lines) {}
    virtual void OnAnsibleSoftwareConfigurationFinished(
        const guest_os::GuestId& container_id,
        bool success) = 0;
    virtual void OnAnsibleSoftwareInstall(
        const guest_os::GuestId& container_id) {}
    virtual void OnApplyAnsiblePlaybook(const guest_os::GuestId& container_id) {
    }
    // Mainly for testing purposes only. Signals observers that the UI element
    // is ready for interaction for a particular configuration task.
    virtual void OnAnsibleSoftwareConfigurationUiPrompt(
        const guest_os::GuestId& container_id,
        bool interactive) {}
  };

  explicit AnsibleManagementService(Profile* profile);

  AnsibleManagementService(const AnsibleManagementService&) = delete;
  AnsibleManagementService& operator=(const AnsibleManagementService&) = delete;

  ~AnsibleManagementService() override;

  // Preconfigures a container with a specified Ansible playbook.
  virtual void ConfigureContainer(
      const guest_os::GuestId& container_id,
      base::FilePath playbook,
      base::OnceCallback<void(bool success)> callback);

  // LinuxPackageOperationProgressObserver:
  void OnInstallLinuxPackageProgress(const guest_os::GuestId& container_id,
                                     InstallLinuxPackageProgressStatus status,
                                     int progress_percent,
                                     const std::string& error_message) override;
  void OnUninstallPackageProgress(const guest_os::GuestId& container_id,
                                  UninstallPackageProgressStatus status,
                                  int progress_percent) override;

  virtual void OnApplyAnsiblePlaybookProgress(
      const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal);

  // Gets the input from the user-facing dialog to determine whether or not a
  // configuration task should be retried.
  void RetryConfiguration(const guest_os::GuestId& container_id);
  void CancelConfiguration(const guest_os::GuestId& container_id);
  void CompleteConfiguration(const guest_os::GuestId& container_id,
                             bool success);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Sets up and returns a mock instance of AnsibleManagementService.
  static AnsibleManagementService* SetUpMockAnsibleManagementServiceForTesting(
      Profile* profile);
  views::Widget* GetDialogWidgetForTesting(
      const guest_os::GuestId& container_id);
  void AddConfigurationTaskForTesting(const guest_os::GuestId& container_id,
                                      views::Widget* widget);

 private:
  bool IsCancelled(const guest_os::GuestId& container_id);

  // Helper function to create the UI elements needed. We won't need to keep
  // track of this because it'll be self-destructing.
  void CreateUiElement(const guest_os::GuestId& container_id);

  void OnInstallAnsibleInContainer(const guest_os::GuestId& container_id,
                                   CrostiniResult result);
  void GetAnsiblePlaybookToApply(const guest_os::GuestId& container_id);
  void OnAnsiblePlaybookRetrieved(const guest_os::GuestId& container_id,
                                  bool success);
  void ApplyAnsiblePlaybook(const guest_os::GuestId& container_id);
  void OnApplyAnsiblePlaybook(
      const guest_os::GuestId& container_id,
      std::optional<vm_tools::cicerone::ApplyAnsiblePlaybookResponse> response);

  // Helper function that runs relevant callback and notifies observers.
  void OnConfigurationFinished(const guest_os::GuestId& container_id,
                               bool success);

  raw_ptr<Profile> profile_;
  base::ObserverList<Observer> observers_;
  std::map<guest_os::GuestId, std::unique_ptr<AnsibleConfiguration>>
      configuration_tasks_;

  // We don't really need to know about these, but keeping them so we can access
  // for testing purposes.
  std::map<guest_os::GuestId, raw_ptr<views::Widget, CtnExperimental>>
      ui_elements_;

  base::WeakPtrFactory<AnsibleManagementService> weak_ptr_factory_;
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_ANSIBLE_ANSIBLE_MANAGEMENT_SERVICE_H_
