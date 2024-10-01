// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crostini/crostini_low_disk_notification.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-forward.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/termina_installer.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_launcher.h"
#include "chrome/browser/ash/guest_os/guest_os_remover.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/public/guest_os_mount_provider_registry.h"
#include "chrome/browser/ash/guest_os/public/guest_os_terminal_provider_registry.h"
#include "chrome/browser/ash/guest_os/vm_shutdown_observer.h"
#include "chrome/browser/ash/guest_os/vm_starting_observer.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector.pb.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {
class NetworkState;
class NetworkStateHandler;
}  // namespace ash

namespace guest_os {
class GuestOsStabilityMonitor;
}  // namespace guest_os

namespace crostini {

extern const char kCrostiniStabilityHistogram[];

class CrostiniUpgradeAvailableNotification;
class CrostiniSshfs;

class LinuxPackageOperationProgressObserver {
 public:
  // A successfully started package install will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED. The
  // |progress_percent| field is given as a percentage of the given step,
  // DOWNLOADING or INSTALLING. If |status| is FAILED, the |error_message|
  // will contain output of the failing installation command.
  virtual void OnInstallLinuxPackageProgress(
      const guest_os::GuestId& container_id,
      InstallLinuxPackageProgressStatus status,
      int progress_percent,
      const std::string& error_message) = 0;

  // A successfully started package uninstall will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED.
  virtual void OnUninstallPackageProgress(const guest_os::GuestId& container_id,
                                          UninstallPackageProgressStatus status,
                                          int progress_percent) = 0;
};

class PendingAppListUpdatesObserver : public base::CheckedObserver {
 public:
  // Called whenever the kPendingAppListUpdatesMethod signal is sent.
  virtual void OnPendingAppListUpdates(const guest_os::GuestId& container_id,
                                       int count) = 0;
};

class ExportContainerProgressObserver {
 public:
  // A successfully started container export will continually fire progress
  // events until the original callback from ExportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_EXPORT_FAILED.
  virtual void OnExportContainerProgress(
      const guest_os::GuestId& container_id,
      const StreamingExportStatus& status) = 0;
};

class ImportContainerProgressObserver {
 public:
  // A successfully started container import will continually fire progress
  // events until the original callback from ImportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_IMPORT_FAILED[_*].
  virtual void OnImportContainerProgress(
      const guest_os::GuestId& container_id,
      ImportContainerProgressStatus status,
      int progress_percent,
      uint64_t progress_speed,
      const std::string& architecture_device,
      const std::string& architecture_container,
      uint64_t available_space,
      uint64_t minimum_required_space) = 0;
};

class DiskImageProgressObserver {
 public:
  // A successfully started container export will continually fire progress
  // events until the original callback from ExportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_EXPORT_FAILED.
  virtual void OnDiskImageProgress(const guest_os::GuestId& container_id,
                                   DiskImageProgressStatus status,
                                   int progress) = 0;
};

class UpgradeContainerProgressObserver {
 public:
  virtual void OnUpgradeContainerProgress(
      const guest_os::GuestId& container_id,
      UpgradeContainerProgressStatus status,
      const std::vector<std::string>& messages) = 0;
};

class CrostiniDialogStatusObserver : public base::CheckedObserver {
 public:
  // Called when a Crostini dialog (installer, upgrader, etc.) opens or
  // closes.
  virtual void OnCrostiniDialogStatusChanged(DialogType dialog_type,
                                             bool open) = 0;
};

class CrostiniContainerPropertiesObserver : public base::CheckedObserver {
 public:
  // Called when a container's OS release version changes.
  virtual void OnContainerOsReleaseChanged(
      const guest_os::GuestId& container_id,
      bool can_upgrade) = 0;
};

class ContainerShutdownObserver : public base::CheckedObserver {
 public:
  // Called when the container has shutdown.
  virtual void OnContainerShutdown(const guest_os::GuestId& container_id) = 0;
};

// CrostiniManager is a singleton which is used to check arguments for
// ConciergeClient and CiceroneClient. ConciergeClient is dedicated to
// communication with the Concierge service, CiceroneClient is dedicated to
// communication with the Cicerone service and both should remain as thin as
// possible. The existence of Cicerone is abstracted behind this class and
// only the Concierge name is exposed outside of here.
class CrostiniManager : public KeyedService,
                        public ash::AnomalyDetectorClient::Observer,
                        public ash::ConciergeClient::VmObserver,
                        public ash::ConciergeClient::DiskImageObserver,
                        public ash::CiceroneClient::Observer,
                        public ash::NetworkStateHandlerObserver,
                        public chromeos::PowerManagerClient::Observer {
 public:
  using CrostiniResultCallback =
      base::OnceCallback<void(CrostiniResult result)>;
  using ExportLxdContainerResultCallback =
      base::OnceCallback<void(CrostiniResult result,
                              uint64_t container_size,
                              uint64_t compressed_size)>;
  // Callback indicating success or failure
  using BoolCallback = base::OnceCallback<void(bool success)>;

  using RestartId = int;
  static const RestartId kUninitializedRestartId = -1;

  // Observer class for the Crostini restart flow.
  class RestartObserver {
   public:
    virtual ~RestartObserver() = default;
    virtual void OnStageStarted(mojom::InstallerState stage) {}
    virtual void OnDiskImageCreated(bool success,
                                    CrostiniResult result,
                                    int64_t disk_size_bytes) {}
    virtual void OnContainerDownloading(int32_t download_percent) {}
  };

  struct RestartOptions {
    RestartSource restart_source = RestartSource::kOther;
    bool start_vm_only = false;
    bool stop_after_lxd_available = false;
    // Paths to share with VM on startup.
    std::vector<base::FilePath> share_paths;
    // These five options only affect new containers.
    std::optional<std::string> container_username;
    std::optional<int64_t> disk_size_bytes;
    std::optional<std::string> image_server_url;
    std::optional<std::string> image_alias;
    std::optional<base::FilePath> ansible_playbook;

    RestartOptions();
    ~RestartOptions();
    // Add copy version if necessary.
    RestartOptions(RestartOptions&&);
    RestartOptions& operator=(RestartOptions&&);
  };

  static CrostiniManager* GetForProfile(Profile* profile);

  explicit CrostiniManager(Profile* profile);

  CrostiniManager(const CrostiniManager&) = delete;
  CrostiniManager& operator=(const CrostiniManager&) = delete;

  ~CrostiniManager() override;

  base::WeakPtr<CrostiniManager> GetWeakPtr();

  // Returns true if the /dev/kvm directory is present.
  static bool IsDevKvmPresent();

  // Returns true if concierge allows termina VM to be launched.
  static bool IsVmLaunchAllowed();

  // Upgrades cros-termina component if the current version is not compatible.
  // This is a no-op if `ash::features::kCrostiniUseDlc` is enabled.
  void MaybeUpdateCrostini();

  // Installs termina using the DLC service.
  void InstallTermina(CrostiniResultCallback callback);

  // Try to cancel a previous InstallTermina call. This is done on a best-effort
  // basis. The callback passed to InstallTermina is still run upon completion.
  void CancelInstallTermina();

  // Unloads and removes termina.
  void UninstallTermina(BoolCallback callback);

  // Checks the arguments for creating a new Termina VM disk image. Creates a
  // disk image for a Termina VM via ConciergeClient::CreateDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  using CreateDiskImageCallback =
      base::OnceCallback<void(CrostiniResult result,
                              const base::FilePath& disk_path)>;
  void CreateDiskImage(
      // The path to the disk image, including the name of
      // the image itself. The image name should match the
      // name of the VM that it will be used for.
      const std::string& vm_name,
      // The storage location for the disk image
      vm_tools::concierge::StorageLocation storage_location,
      // The logical size of the disk image, in bytes
      int64_t disk_size_bytes,
      CreateDiskImageCallback callback);

  // Checks the arguments for starting a Termina VM. Starts a Termina VM via
  // ConciergeClient::StartTerminaVm. |callback| is called if the arguments
  // are bad, or after the method call finishes.
  void StartTerminaVm(
      // The human-readable name to be assigned to this VM.
      std::string name,
      // Path to the disk image on the host.
      const base::FilePath& disk_path,
      // The number of logical CPU cores that are currently disabled.
      size_t num_cores_disabled,
      // A callback to invoke with the result of the launch request.
      BoolCallback callback);

  // Checks the arguments for stopping a Termina VM. Stops the Termina VM via
  // ConciergeClient::StopVm. |callback| is called if the arguments are bad,
  // or after the method call finishes.
  void StopVm(std::string name, CrostiniResultCallback callback);

  // Calls |StopVm| for each member of |running_vms_| not already in state
  // |STOPPING|.
  void StopRunningVms(CrostiniResultCallback callback);

  // Asynchronously retrieve the Termina VM kernel version using concierge's
  // GetVmEnterpriseReportingInfo method and store it in prefs.
  void UpdateTerminaVmKernelVersion();

  // Wrapper for CiceroneClient::StartLxd with some extra parameter validation.
  // |callback| is called immediately if the arguments are bad, or after LXD has
  // been started.
  void StartLxd(std::string vm_name, CrostiniResultCallback callback);

  // Checks the arguments for creating an Lxd container via
  // CiceroneClient::CreateLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been created.
  void CreateLxdContainer(guest_os::GuestId container_id,
                          std::optional<std::string> opt_image_server_url,
                          std::optional<std::string> opt_image_alias,
                          CrostiniResultCallback callback);

  // Checks the arguments for deleting an Lxd container via
  // CiceroneClient::DeleteLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been deleted.
  void DeleteLxdContainer(guest_os::GuestId container_id,
                          BoolCallback callback);

  // Checks the arguments for starting an Lxd container via
  // CiceroneClient::StartLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been created.
  void StartLxdContainer(guest_os::GuestId container_id,
                         CrostiniResultCallback callback);

  // Checks the arguments for stopping an Lxd container via
  // CiceroneClient::StopLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been stopped.
  void StopLxdContainer(guest_os::GuestId container_id,
                        CrostiniResultCallback callback);

  // Checks the arguments for setting up an Lxd container user via
  // CiceroneClient::SetUpLxdContainerUser. |callback| is called immediately if
  // the arguments are bad, or once garcon has been started.
  void SetUpLxdContainerUser(guest_os::GuestId container_id,
                             std::string container_username,
                             BoolCallback callback);

  // Checks the arguments for exporting a vm disk image via
  // ConciergeClient::ExportDiskImage. |callback| is called immedaitely if the
  // arguments are bad, or after the method call finishes.
  // using DiskImageCallback = base::OnceCallback<void(CrostiniResult result)>;
  void ExportDiskImage(guest_os::GuestId vm_id,
                       std::string user_id_hash,
                       base::FilePath export_path,
                       bool force,
                       CrostiniResultCallback callback);

  // Checks the arguments for exporting a vm disk image via
  // ConciergeClient::ImportDiskImage. |callback| is called immedaitely if the
  // arguments are bad, or after the method call finishes.
  // using DiskImageCallback = base::OnceCallback<void(CrostiniResult result)>;
  void ImportDiskImage(guest_os::GuestId vm_id,
                       std::string user_id_hash,
                       base::FilePath import_path,
                       CrostiniResultCallback callback);

  // Checks the arguments for exporting an Lxd container via
  // CiceroneClient::ExportLxdContainer. |callback| is called immediately if the
  // arguments are bad, or after the method call finishes.
  void ExportLxdContainer(guest_os::GuestId container_id,
                          base::FilePath export_path,
                          ExportLxdContainerResultCallback callback);

  // Checks the arguments for importing an Lxd container via
  // CiceroneClient::ImportLxdContainer. |callback| is called immediately if the
  // arguments are bad, or after the method call finishes.
  void ImportLxdContainer(guest_os::GuestId container_id,
                          base::FilePath import_path,
                          CrostiniResultCallback callback);

  // Checks the arguments for cancelling a disk image export via
  // ConciergeClient::CancelExportDiskImage.
  void CancelDiskImageOp(guest_os::GuestId key);

  // Checks the arguments for cancelling a Lxd container export via
  // CiceroneClient::CancelExportLxdContainer .
  void CancelExportLxdContainer(guest_os::GuestId key);

  // Checks the arguments for cancelling a Lxd container import via
  // CiceroneClient::CancelImportLxdContainer.
  void CancelImportLxdContainer(guest_os::GuestId key);

  // Checks the arguments for upgrading an existing container via
  // CiceroneClient::UpgradeContainer. An UpgradeProgressObserver should be used
  // to monitor further results.
  void UpgradeContainer(const guest_os::GuestId& key,
                        ContainerVersion target_version,
                        CrostiniResultCallback callback);

  // Checks the arguments for canceling the upgrade of an existing container via
  // CiceroneClient::CancelUpgradeContainer.
  void CancelUpgradeContainer(const guest_os::GuestId& key,
                              CrostiniResultCallback callback);

  // Asynchronously gets app icons as specified by their desktop file ids.
  // |callback| is called after the method call finishes.
  using GetContainerAppIconsCallback =
      base::OnceCallback<void(bool success, const std::vector<Icon>& icons)>;
  void GetContainerAppIcons(const guest_os::GuestId& container_id,
                            std::vector<std::string> desktop_file_ids,
                            int icon_size,
                            int scale,
                            GetContainerAppIconsCallback callback);

  // Asynchronously retrieve information about a Linux Package (.deb) inside the
  // container.
  using GetLinuxPackageInfoCallback =
      base::OnceCallback<void(const LinuxPackageInfo&)>;
  void GetLinuxPackageInfo(const guest_os::GuestId& container_id,
                           std::string package_path,
                           GetLinuxPackageInfoCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers.
  using InstallLinuxPackageCallback = CrostiniResultCallback;
  void InstallLinuxPackage(const guest_os::GuestId& container_id,
                           std::string package_path,
                           InstallLinuxPackageCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers. Uses a package_id, given
  // by "package_name;version;arch;data", to identify the package to install
  // from the APT repository.
  void InstallLinuxPackageFromApt(const guest_os::GuestId& container_id,
                                  std::string package_id,
                                  InstallLinuxPackageCallback callback);

  // Begin uninstallation of a Linux Package inside the container. The package
  // is identified by its associated .desktop file's ID; we don't use package_id
  // to avoid problems with stale package_ids (such as after upgrades). If the
  // uninstallation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers.
  void UninstallPackageOwningFile(const guest_os::GuestId& container_id,
                                  std::string desktop_file_id,
                                  CrostiniResultCallback callback);

  // Runs all the steps required to restart the given crostini vm and container.
  // The optional |observer| tracks progress. If provided, it must be alive
  // until the restart completes (i.e. when |callback| is called) or the request
  // is cancelled via |CancelRestartCrostini|.
  RestartId RestartCrostini(guest_os::GuestId container_id,
                            CrostiniResultCallback callback,
                            RestartObserver* observer = nullptr);

  RestartId RestartCrostiniWithOptions(guest_os::GuestId container_id,
                                       RestartOptions options,
                                       CrostiniResultCallback callback,
                                       RestartObserver* observer = nullptr);

  // CreateOption operations.

  // Registers the CreateOptions to create a container with specified
  // RestartOptions. For containers that existed before this feature, this will
  // be generic restart options, for newly created containers, this will store
  // the initial starting information. Returns false if there is already an
  // CreateOption registered.
  bool RegisterCreateOptions(const guest_os::GuestId& container_id,
                             const RestartOptions& options);

  // Fetches the CreateOptions as RestartOptions. Returns True if this
  // configuration has been started with before.
  bool FetchCreateOptions(const guest_os::GuestId& container_id,
                          RestartOptions* restart_options);

  // Returns true if the container is currently pending creation.
  bool IsPendingCreation(const guest_os::GuestId& container_id);

  // Sets an CreateOptions as booted, so it becomes a historical record and has
  // no effect on future starts.
  void SetCreateOptionsUsed(const guest_os::GuestId& container_id);

  // Cancel a restart request. The associated result callback will be fired
  // immediately and the observer will be removed. If there were multiple
  // restart requests for the same container id, the restart may actually keep
  // going.
  void CancelRestartCrostini(RestartId restart_id);

  // Returns true if the Restart corresponding to |restart_id| is not yet
  // complete.
  bool IsRestartPending(RestartId restart_id);

  // Returns whether there is an active restarter for a given GuestId. Even
  // after cancelling all requests or aborting a restarter, this will continue
  // to return true until the current operation is finished and the restarter
  // is destroyed.
  bool HasRestarterForTesting(const guest_os::GuestId& guest_id);

  // Adds a callback to receive notification of container shutdown.
  void AddShutdownContainerCallback(guest_os::GuestId container_id,
                                    base::OnceClosure shutdown_callback);

  // Adds a callback to receive uninstall notification.
  using RemoveCrostiniCallback = CrostiniResultCallback;
  void AddRemoveCrostiniCallback(RemoveCrostiniCallback remove_callback);

  // Add/remove observers for package install and uninstall progress.
  void AddLinuxPackageOperationProgressObserver(
      LinuxPackageOperationProgressObserver* observer);
  void RemoveLinuxPackageOperationProgressObserver(
      LinuxPackageOperationProgressObserver* observer);

  // Add/remove observers for pending app list updates.
  void AddPendingAppListUpdatesObserver(
      PendingAppListUpdatesObserver* observer);
  void RemovePendingAppListUpdatesObserver(
      PendingAppListUpdatesObserver* observer);

  // Add/remove observers for container export/import.
  void AddExportContainerProgressObserver(
      ExportContainerProgressObserver* observer);
  void RemoveExportContainerProgressObserver(
      ExportContainerProgressObserver* observer);
  void AddImportContainerProgressObserver(
      ImportContainerProgressObserver* observer);
  void RemoveImportContainerProgressObserver(
      ImportContainerProgressObserver* observer);

  // Add/remove observers for disk image export/import
  void AddDiskImageProgressObserver(DiskImageProgressObserver* observer);
  void RemoveDiskImageProgressObserver(DiskImageProgressObserver* observer);

  // Add/remove observers for container upgrade
  void AddUpgradeContainerProgressObserver(
      UpgradeContainerProgressObserver* observer);
  void RemoveUpgradeContainerProgressObserver(
      UpgradeContainerProgressObserver* observer);

  // Add/remove vm shutdown observers.
  void AddVmShutdownObserver(ash::VmShutdownObserver* observer);
  void RemoveVmShutdownObserver(ash::VmShutdownObserver* observer);

  // Add/remove vm starting observers.
  void AddVmStartingObserver(ash::VmStartingObserver* observer);
  void RemoveVmStartingObserver(ash::VmStartingObserver* observer);

  // AnomalyDetectorClient::Observer:
  void OnGuestFileCorruption(
      const anomaly_detector::GuestFileCorruptionSignal& signal) override;

  // ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;
  void OnVmStopping(
      const vm_tools::concierge::VmStoppingSignal& signal) override;

  // ConciergeClient::DiskImageObserver
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override;

  // CiceroneClient::Observer:
  void OnContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal) override;
  void OnContainerShutdown(
      const vm_tools::cicerone::ContainerShutdownSignal& signal) override;
  void OnInstallLinuxPackageProgress(
      const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal)
      override;
  void OnUninstallPackageProgress(
      const vm_tools::cicerone::UninstallPackageProgressSignal& signal)
      override;
  void OnLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal) override;
  void OnLxdContainerDeleted(
      const vm_tools::cicerone::LxdContainerDeletedSignal& signal) override;
  void OnLxdContainerDownloading(
      const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) override;
  void OnTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal) override;
  void OnLxdContainerStarting(
      const vm_tools::cicerone::LxdContainerStartingSignal& signal) override;
  void OnExportLxdContainerProgress(
      const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal)
      override;
  void OnImportLxdContainerProgress(
      const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal)
      override;
  void OnPendingAppListUpdates(
      const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) override;
  void OnApplyAnsiblePlaybookProgress(
      const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal)
      override;
  void OnUpgradeContainerProgress(
      const vm_tools::cicerone::UpgradeContainerProgressSignal& signal)
      override;
  void OnStartLxdProgress(
      const vm_tools::cicerone::StartLxdProgressSignal& signal) override;

  // ash::NetworkStateHandlerObserver overrides:
  void ActiveNetworksChanged(
      const std::vector<const ash::NetworkState*>& active_networks) override;
  void OnShuttingDown() override;

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Callback for |RemoveSshfsCrostiniVolume| called from |SuspendImminent| when
  // the device is allowed to suspend. Removes metadata associated with the
  // crostini sshfs mount and unblocks a pending suspend.
  void OnRemoveSshfsCrostiniVolume(
      base::UnguessableToken power_manager_suspend_token,
      bool result);

  void RemoveCrostini(std::string vm_name, RemoveCrostiniCallback callback);

  void UpdateVmState(std::string vm_name, VmState vm_state);
  bool IsVmRunning(std::string vm_name);
  // Returns std::nullopt if VM is not running.
  std::optional<VmInfo> GetVmInfo(std::string vm_name);
  void AddRunningVmForTesting(std::string vm_name, uint32_t cid = 0);
  void AddStoppingVmForTesting(std::string vm_name);

  void SetContainerOsRelease(const guest_os::GuestId& container_id,
                             const vm_tools::cicerone::OsRelease& os_release);
  const vm_tools::cicerone::OsRelease* GetContainerOsRelease(
      const guest_os::GuestId& container_id) const;
  void AddRunningContainerForTesting(std::string vm_name,
                                     ContainerInfo info,
                                     bool notify = false);

  // If the Crostini reporting policy is set, save the last app launch
  // time window and the Termina version in prefs for asynchronous reporting.
  void UpdateLaunchMetricsForEnterpriseReporting();

  // Clear the lists of running VMs and containers.
  // Can be called for testing to skip restart.
  void set_skip_restart_for_testing() { skip_restart_for_testing_ = true; }
  bool skip_restart_for_testing() { return skip_restart_for_testing_; }

  void SetCrostiniDialogStatus(DialogType dialog_type, bool open);
  // Returns true if the dialog is open.
  bool GetCrostiniDialogStatus(DialogType dialog_type) const;
  void AddCrostiniDialogStatusObserver(CrostiniDialogStatusObserver* observer);
  void RemoveCrostiniDialogStatusObserver(
      CrostiniDialogStatusObserver* observer);

  void AddCrostiniContainerPropertiesObserver(
      CrostiniContainerPropertiesObserver* observer);
  void RemoveCrostiniContainerPropertiesObserver(
      CrostiniContainerPropertiesObserver* observer);

  void AddContainerShutdownObserver(ContainerShutdownObserver* observer);
  void RemoveContainerShutdownObserver(ContainerShutdownObserver* observer);

  bool IsContainerUpgradeable(const guest_os::GuestId& container_id) const;
  bool ShouldPromptContainerUpgrade(
      const guest_os::GuestId& container_id) const;
  void UpgradePromptShown(const guest_os::GuestId& container_id);
  bool IsUncleanStartup() const;
  void SetUncleanStartupForTesting(bool is_unclean_startup);
  void RemoveUncleanSshfsMounts();
  void DeallocateForwardedPortsCallback(const guest_os::GuestId& container_id);

  void CallRestarterStartLxdContainerFinishedForTesting(
      CrostiniManager::RestartId id,
      CrostiniResult result);
  void SetInstallTerminaNeverCompletesForTesting(bool never_completes) {
    install_termina_never_completes_for_testing_ = never_completes;
  }

  // Mounts the user's Crostini home directory so it's accessible from the host.
  // Must be called from the UI thread, no-op if the home directory is already
  // mounted. If this is something running in the background set background to
  // true, if failures are user-visible set it to false. If you're setting
  // base::DoNothing as the callback then background should be true.
  void MountCrostiniFiles(guest_os::GuestId container_id,
                          CrostiniResultCallback callback,
                          bool background);

  void GetInstallLocation(base::OnceCallback<void(base::FilePath)> callback);

 private:
  class CrostiniRestarter;

  void RemoveDBusObservers();

  // Callback for ConciergeClient::CreateDiskImage. Called after the Concierge
  // service method finishes.
  void OnCreateDiskImage(
      CreateDiskImageCallback callback,
      std::optional<vm_tools::concierge::CreateDiskImageResponse> response);

  // Callback for ConciergeClient::StartVm. Called after the Concierge
  // service method finishes.  Updates running containers list then calls the
  // |callback| if the container has already been started, otherwise passes the
  // callback to OnStartTremplin.
  void OnStartTerminaVm(
      std::string vm_name,
      BoolCallback callback,
      std::optional<vm_tools::concierge::StartVmResponse> response);

  // Callback for ConciergeClient::TremplinStartedSignal. Called after the
  // Tremplin service starts. Updates running containers list and then calls the
  // |callback| with true, indicating success.
  void OnStartTremplin(std::string vm_name,
                       uint32_t seneschal_server_handle,
                       BoolCallback callback);

  // Callback for ConciergeClient::StopVm. Called after the Concierge
  // service method finishes.
  void OnStopVm(std::string vm_name,
                CrostiniResultCallback callback,
                std::optional<vm_tools::concierge::StopVmResponse> response);

  // Callback for ConciergeClient::GetVmEnterpriseReportingInfo.
  // Currently used to report the Termina kernel version for enterprise
  // reporting.
  void OnGetTerminaVmKernelVersion(
      std::optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
          response);

  // Callback for CiceroneClient::StartLxd. May indicate that LXD is still being
  // started in which case we will wait for OnStartLxdProgress events.
  void OnStartLxd(std::string vm_name,
                  CrostiniResultCallback callback,
                  std::optional<vm_tools::cicerone::StartLxdResponse> response);

  // Callback for ConciergeClient::ExportDiskImage. Called after the Concierge
  // service method finishes.
  void OnExportDiskImage(
      guest_os::GuestId vm_id,
      std::optional<vm_tools::concierge::ExportDiskImageResponse> response);

  // Callback for ConciergeClient::ImportDiskImage. Called after the Concierge
  // service method finishes.
  void OnImportDiskImage(
      guest_os::GuestId vm_id,
      std::optional<vm_tools::concierge::ImportDiskImageResponse> response);

  // Callback for CiceroneClient::CreateLxdContainer. May indicate the container
  // is still being created, in which case we will wait for an
  // OnLxdContainerCreated event.
  void OnCreateLxdContainer(
      const guest_os::GuestId& container_id,
      CrostiniResultCallback callback,
      std::optional<vm_tools::cicerone::CreateLxdContainerResponse> response);

  // Callback for CiceroneClient::DeleteLxdContainer.
  void OnDeleteLxdContainer(
      const guest_os::GuestId& container_id,
      BoolCallback callback,
      std::optional<vm_tools::cicerone::DeleteLxdContainerResponse> response);

  // Callback for CiceroneClient::StartLxdContainer.
  void OnStartLxdContainer(
      const guest_os::GuestId& container_id,
      CrostiniResultCallback callback,
      std::optional<vm_tools::cicerone::StartLxdContainerResponse> response);

  // Callback for CiceroneClient::StopLxdContainer.
  void OnStopLxdContainer(
      const guest_os::GuestId& container_id,
      CrostiniResultCallback callback,
      std::optional<vm_tools::cicerone::StopLxdContainerResponse> response);

  // Callback for CiceroneClient::SetUpLxdContainerUser.
  void OnSetUpLxdContainerUser(
      const guest_os::GuestId& container_id,
      BoolCallback callback,
      std::optional<vm_tools::cicerone::SetUpLxdContainerUserResponse>
          response);

  // Callback for CiceroneClient::ExportLxdContainer.
  void OnExportLxdContainer(
      const guest_os::GuestId& container_id,
      std::optional<vm_tools::cicerone::ExportLxdContainerResponse> response);

  // Callback for CiceroneClient::ImportLxdContainer.
  void OnImportLxdContainer(
      const guest_os::GuestId& container_id,
      std::optional<vm_tools::cicerone::ImportLxdContainerResponse> response);

  // Callback for CiceroneClient::CancelExportDiskImage.
  void OnCancelDiskImageOp(
      const guest_os::GuestId& key,
      std::optional<vm_tools::concierge::CancelDiskImageResponse> response);

  // Callback for CiceroneClient::CancelExportLxdContainer.
  void OnCancelExportLxdContainer(
      const guest_os::GuestId& key,
      std::optional<vm_tools::cicerone::CancelExportLxdContainerResponse>
          response);

  // Callback for CiceroneClient::CancelImportLxdContainer.
  void OnCancelImportLxdContainer(
      const guest_os::GuestId& key,
      std::optional<vm_tools::cicerone::CancelImportLxdContainerResponse>
          response);

  // Callback for CiceroneClient::UpgradeContainer.
  void OnUpgradeContainer(
      CrostiniResultCallback callback,
      std::optional<vm_tools::cicerone::UpgradeContainerResponse> response);

  // Callback for CiceroneClient::CancelUpgradeContainer.
  void OnCancelUpgradeContainer(
      CrostiniResultCallback callback,
      std::optional<vm_tools::cicerone::CancelUpgradeContainerResponse>
          response);

  // Callback for CrostiniManager::LaunchContainerApplication.
  void OnLaunchContainerApplication(
      guest_os::launcher::SuccessCallback callback,
      std::optional<vm_tools::cicerone::LaunchContainerApplicationResponse>
          response);

  // Callback for CrostiniManager::GetContainerAppIcons. Called after the
  // Concierge service finishes.
  void OnGetContainerAppIcons(
      GetContainerAppIconsCallback callback,
      std::optional<vm_tools::cicerone::ContainerAppIconResponse> response);

  // Callback for CrostiniManager::GetLinuxPackageInfo.
  void OnGetLinuxPackageInfo(
      GetLinuxPackageInfoCallback callback,
      std::optional<vm_tools::cicerone::LinuxPackageInfoResponse> response);

  // Callback for CrostiniManager::InstallLinuxPackage.
  void OnInstallLinuxPackage(
      InstallLinuxPackageCallback callback,
      std::optional<vm_tools::cicerone::InstallLinuxPackageResponse> response);

  // Callback for CrostiniManager::UninstallPackageOwningFile.
  void OnUninstallPackageOwningFile(
      CrostiniResultCallback callback,
      std::optional<vm_tools::cicerone::UninstallPackageOwningFileResponse>
          response);

  // Helper for CrostiniManager::MaybeUpdateCrostini. Makes blocking calls to
  // check for /dev/kvm.
  static void CheckPaths();

  // Helper for CrostiniManager::MaybeUpdateCrostini. Checks that concierge is
  // available.
  void CheckConciergeAvailable();

  // Helper for CrostiniManager::MaybeUpdateCrostini. Checks that concierge will
  // allow the termina VM to be launched.
  void CheckVmLaunchAllowed(bool service_is_available);
  void OnCheckVmLaunchAllowed(
      std::optional<vm_tools::concierge::GetVmLaunchAllowedResponse> response);

  // Helper for CrostiniManager::MaybeUpdateCrostini. Separated because the
  // checking component registration code may block.
  void MaybeUpdateCrostiniAfterChecks();

  // Called by CrostiniRestarter once it's done with a specific restart request.
  void RemoveRestartId(RestartId restart_id);
  // Called by CrostiniRestarter once it's finished. |closure| encapsulates any
  // outstanding callbacks passed to RestartCrostini*().
  void RestartCompleted(CrostiniRestarter* restarter,
                        base::OnceClosure closure);

  // Callback for CrostiniManager::RemoveCrostini.
  void OnRemoveCrostini(guest_os::GuestOsRemover::Result result);

  void OnRemoveTermina(bool success);

  void FinishUninstall(CrostiniResult result);

  void OnVmStoppedCleanup(const std::string& vm_name);

  // Configure the container so that it can sideload apps into Arc++.
  void ConfigureForArcSideload();

  // Tries to query Concierge for the type of disk the named VM has then emits a
  // metric logging the type. Mostly happens async and best-effort.
  void EmitVmDiskTypeMetric(const std::string vm_name);

  // Runs things that should happened whenever a container shutdowns e.g.
  // triggering observers.
  void HandleContainerShutdown(const guest_os::GuestId& container_id);

  // Registers a container with the GuestOsService's terminal provider registry.
  void RegisterContainerTerminal(const guest_os::GuestId& container_id);

  // Registers a container with GuestOsService's registries. No-op if it's
  // already registered.
  void RegisterContainer(const guest_os::GuestId& container_id);

  // Unregisters a container from GuestOsService's registries. No-op if it's
  // not registered.
  void UnregisterContainer(const guest_os::GuestId& container_id);

  // Unregisters all container from GuestOsService's registries.
  void UnregisterAllContainers();

  // Best-effort attempt to premount the user's files.
  void MountCrostiniFilesBackground(guest_os::GuestInfo info);

  bool ShouldWarnAboutExpiredVersion(const guest_os::GuestId& container_id);

  raw_ptr<Profile> profile_;
  std::string owner_id_;

  bool skip_restart_for_testing_ = false;

  static bool is_dev_kvm_present_;
  static bool is_vm_launch_allowed_;

  // |is_unclean_startup_| is true when we detect Concierge still running at
  // session startup time, and the last session ended in a crash.
  bool is_unclean_startup_ = false;

  // Callbacks that are waiting on a signal
  std::multimap<guest_os::GuestId, CrostiniResultCallback>
      start_container_callbacks_;
  std::multimap<guest_os::GuestId, base::OnceClosure>
      shutdown_container_callbacks_;
  std::multimap<guest_os::GuestId, CrostiniResultCallback>
      create_lxd_container_callbacks_;
  std::multimap<guest_os::GuestId, BoolCallback>
      delete_lxd_container_callbacks_;
  std::map<guest_os::GuestId, ExportLxdContainerResultCallback>
      export_lxd_container_callbacks_;
  std::map<guest_os::GuestId, CrostiniResultCallback> disk_image_callbacks_;
  std::map<std::string, guest_os::GuestId> disk_image_uuid_to_guest_id_;
  std::map<guest_os::GuestId, CrostiniResultCallback>
      import_lxd_container_callbacks_;

  // Callbacks to run after Tremplin is started, keyed by vm_name. These are
  // used if StartTerminaVm completes but we need to wait from Tremplin to
  // start.
  std::multimap<std::string, base::OnceClosure> tremplin_started_callbacks_;

  // Callbacks to run after LXD is started, keyed by vm_name. Used if StartLxd
  // completes but we need to wait for LXD to start.
  std::multimap<std::string, CrostiniResultCallback> start_lxd_callbacks_;

  std::map<std::string, VmInfo> running_vms_;

  // OsRelease protos keyed by guest_os::GuestId. We populate this map even if a
  // container fails to start normally.
  std::map<guest_os::GuestId, vm_tools::cicerone::OsRelease>
      container_os_releases_;
  std::set<guest_os::GuestId> container_upgrade_prompt_shown_;

  std::vector<RemoveCrostiniCallback> remove_crostini_callbacks_;

  base::ObserverList<LinuxPackageOperationProgressObserver>::
      UncheckedAndDanglingUntriaged linux_package_operation_progress_observers_;

  base::ObserverList<PendingAppListUpdatesObserver>
      pending_app_list_updates_observers_;

  base::ObserverList<ExportContainerProgressObserver>::
      UncheckedAndDanglingUntriaged export_container_progress_observers_;
  base::ObserverList<ImportContainerProgressObserver>::
      UncheckedAndDanglingUntriaged import_container_progress_observers_;

  base::ObserverList<DiskImageProgressObserver>::UncheckedAndDanglingUntriaged
      disk_image_progress_observers_;

  base::ObserverList<UpgradeContainerProgressObserver>::
      UncheckedAndDanglingUntriaged upgrade_container_progress_observers_;

  base::ObserverList<ash::VmShutdownObserver> vm_shutdown_observers_;
  base::ObserverList<ash::VmStartingObserver> vm_starting_observers_;

  // RestartIds present in |restarters_by_id_| will always have a restarter in
  // |restarters_by_container_| for the corresponding guest_os::GuestId.
  std::map<CrostiniManager::RestartId, guest_os::GuestId> restarters_by_id_;
  std::map<guest_os::GuestId, std::unique_ptr<CrostiniRestarter>>
      restarters_by_container_;
  static RestartId next_restart_id_;

  base::ObserverList<CrostiniDialogStatusObserver>
      crostini_dialog_status_observers_;
  base::ObserverList<CrostiniContainerPropertiesObserver>
      crostini_container_properties_observers_;

  base::ObserverList<ContainerShutdownObserver> container_shutdown_observers_;

  // Contains the types of crostini dialogs currently open. It is generally
  // invalid to show more than one. e.g. uninstalling and installing are
  // mutually exclusive.
  base::flat_set<DialogType> open_crostini_dialogs_;

  bool dbus_observers_removed_ = false;

  base::Time time_of_last_disk_type_metric_;

  std::unique_ptr<guest_os::GuestOsStabilityMonitor>
      guest_os_stability_monitor_;

  std::unique_ptr<CrostiniLowDiskNotification> low_disk_notifier_;

  std::unique_ptr<CrostiniUpgradeAvailableNotification>
      upgrade_available_notification_;

  TerminaInstaller termina_installer_;

  bool install_termina_never_completes_for_testing_ = false;

  std::unique_ptr<CrostiniSshfs> crostini_sshfs_;

  base::flat_map<guest_os::GuestId,
                 guest_os::GuestOsTerminalProviderRegistry::Id>
      terminal_provider_ids_;

  base::ScopedObservation<ash::NetworkStateHandler,
                          ash::NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  base::flat_map<guest_os::GuestId, guest_os::GuestOsMountProviderRegistry::Id>
      mount_provider_ids_;

  base::CallbackListSubscription primary_counter_mount_subscription_;

  bool already_warned_expired_version_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniManager> weak_ptr_factory_{this};
};

}  // namespace crostini

namespace base {

template <>
struct ScopedObservationTraits<crostini::CrostiniManager,
                               crostini::ContainerShutdownObserver> {
  static void AddObserver(crostini::CrostiniManager* source,
                          crostini::ContainerShutdownObserver* observer) {
    source->AddContainerShutdownObserver(observer);
  }
  static void RemoveObserver(crostini::CrostiniManager* source,
                             crostini::ContainerShutdownObserver* observer) {
    source->RemoveContainerShutdownObserver(observer);
  }
};

template <>
struct ScopedObservationTraits<crostini::CrostiniManager,
                               ash::VmShutdownObserver> {
  static void AddObserver(crostini::CrostiniManager* source,
                          ash::VmShutdownObserver* observer) {
    source->AddVmShutdownObserver(observer);
  }
  static void RemoveObserver(crostini::CrostiniManager* source,
                             ash::VmShutdownObserver* observer) {
    source->RemoveVmShutdownObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_H_
