// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_
#define CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/guest_os/guest_os_dlc_helper.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_license_checker.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/public/mojom/wake_lock.mojom.h"

namespace download {
class BackgroundDownloadService;
struct CompletionInfo;
}  // namespace download

class Profile;

namespace plugin_vm {

class PluginVmDriveImageDownloadService;

// PluginVmInstaller is responsible for installing Plugin VM, including
// downloading the image from url specified by the user policy, and importing
// the downloaded image archive using concierge D-Bus services.
//
// The installation flow is fairly linear. On top of cancelled and failed
// installs, the branches are:
// - OnListVmDisks() exits if a VM already exists (installed via vmc).
// - StartDownload() uses a PluginVmDriveImageDownloadService for images hosted
// on Drive, and the DownloadService for all other images.
// - OnFDPrepared() calls concierge's CreateDiskImage() or ImportDiskImage()
// depending on whether an .iso (new VM) or archive (prepared VM) is
// downloaded.
class PluginVmInstaller : public KeyedService,
                          public ash::ConciergeClient::DiskImageObserver {
 public:
  // FailureReasons values are logged to UMA and shown to users. Do not change
  // or re-use enum values.
  enum class FailureReason {
    // LOGIC_ERROR = 0,
    SIGNAL_NOT_CONNECTED = 1,
    OPERATION_IN_PROGRESS = 2,
    NOT_ALLOWED = 3,
    INVALID_IMAGE_URL = 4,
    UNEXPECTED_DISK_IMAGE_STATUS = 5,
    INVALID_DISK_IMAGE_STATUS_RESPONSE = 6,
    DOWNLOAD_FAILED_UNKNOWN = 7,
    DOWNLOAD_FAILED_NETWORK = 8,
    DOWNLOAD_FAILED_ABORTED = 9,
    HASH_MISMATCH = 10,
    DISPATCHER_NOT_AVAILABLE = 11,
    CONCIERGE_NOT_AVAILABLE = 12,
    COULD_NOT_OPEN_IMAGE = 13,
    INVALID_IMPORT_RESPONSE = 14,
    IMAGE_IMPORT_FAILED = 15,
    // DLC_DOWNLOAD_FAILED = 16,
    // DLC_DOWNLOAD_NOT_STARTED = 17,
    DLC_INTERNAL = 18,
    DLC_UNSUPPORTED = 19,
    DLC_BUSY = 20,
    DLC_NEED_REBOOT = 21,
    DLC_NEED_SPACE = 22,
    INSUFFICIENT_DISK_SPACE = 23,  // Pre-check based on policy.
    INVALID_LICENSE = 24,
    OFFLINE = 25,
    LIST_VM_DISKS_FAILED = 26,
    OUT_OF_DISK_SPACE = 27,    // Hard error, we actually ran out of space.
    DOWNLOAD_FAILED_401 = 28,  // Common HTTP status codes for errors.
    DOWNLOAD_FAILED_403 = 29,
    DOWNLOAD_FAILED_404 = 30,
    // Download appeared to succeed but downloaded image size was unexpected
    DOWNLOAD_SIZE_MISMATCH = 31,
    // Image with the right name exists, but in a wrong location.
    EXISTING_IMAGE_INVALID = 32,

    kMaxValue = EXISTING_IMAGE_INVALID,
  };

  enum class InstallingState {
    kInactive,
    kCheckingLicense,
    kCheckingForExistingVm,
    kCheckingDiskSpace,
    kDownloadingDlc,
    kStartingDispatcher,
    kDownloadingImage,
    kImporting,
  };

  static constexpr int64_t kImageSizeUnknown = -1;
  static constexpr int64_t kImageSizeError = -2;

  // Observer for installation progress.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Fired on transitions to any state aside from kInactive.
    virtual void OnStateUpdated(InstallingState new_state) = 0;

    virtual void OnProgressUpdated(double fraction_complete) = 0;
    virtual void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                           int64_t content_length) = 0;

    // Exactly one of these will be fired once installation has finished,
    // successfully or otherwise.
    virtual void OnVmExists() = 0;
    virtual void OnCreated() = 0;
    virtual void OnImported() = 0;
    virtual void OnError(FailureReason reason) = 0;

    virtual void OnCancelFinished() = 0;
  };

  explicit PluginVmInstaller(Profile* profile);
  PluginVmInstaller(const PluginVmInstaller&) = delete;
  PluginVmInstaller& operator=(const PluginVmInstaller&) = delete;
  ~PluginVmInstaller() override;

  // Start the installation. Progress updates will be sent to the observer.
  // Returns a FailureReason if the installation couldn't be started.
  std::optional<FailureReason> Start();
  // Cancel the installation, and calls OnCancelFinished() when done. Some steps
  // cannot be directly cancelled, in which case we wait for the step to
  // complete and then abort the installation.
  // DLC will not be removed, but the downloaded image will be.
  void Cancel();

  // Returns whether the installer is already running.
  bool IsProcessing();

  void SetObserver(Observer* observer);
  void RemoveObserver();

  std::string GetCurrentDownloadGuid();

  // Used by PluginVmImageDownloadClient and PluginVmDriveImageDownloadService,
  // other classes should not call into here.
  void OnDownloadStarted();
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length);
  void OnDownloadCompleted(const download::CompletionInfo& info);
  void OnDownloadFailed(FailureReason reason);

  // ConciergeClient::DiskImageObserver:
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override;

  // Helper function that returns whether the hash of the downloaded image
  // matches the hash specified in policy.
  // Public for testing purposes.
  bool VerifyDownload(const std::string& download_hash);

  // Returns free disk space required to install Plugin VM in bytes.
  int64_t RequiredFreeDiskSpace();

  void SkipLicenseCheckForTesting() { skip_license_check_for_testing_ = true; }
  void SetFreeDiskSpaceForTesting(int64_t bytes) {
    free_disk_space_for_testing_ = bytes;
  }
  void SetDownloadServiceForTesting(
      download::BackgroundDownloadService* download_service);
  void SetDownloadedImageForTesting(const base::FilePath& downloaded_image);
  void SetDriveDownloadServiceForTesting(
      std::unique_ptr<PluginVmDriveImageDownloadService>
          drive_download_service);

 private:
  enum class State {
    kIdle,
    kInstalling,
    kCancelling,
  };

  // The entire installation flow!

  void CheckLicense();
  void OnLicenseChecked(bool license_is_valid);

  void CheckForExistingVm();
  void OnConciergeAvailable(bool success);
  void OnListVmDisks(
      std::optional<vm_tools::concierge::ListVmDisksResponse> response);

  void CheckDiskSpace();
  void OnAvailableDiskSpace(std::optional<int64_t> bytes);

  void StartDlcDownload();
  // Called repeatedly.
  void OnDlcDownloadProgressUpdated(double progress);
  void OnDlcDownloadCompleted(
      guest_os::GuestOsDlcInstallation::Result install_result);

  void StartDispatcher();
  void OnDispatcherStarted(bool success);

  void StartDownload();
  // This is only called in the DownloadService flow.
  void OnStartDownload(const std::string& download_guid,
                       download::DownloadParams::StartResult start_result);
  // Download progress/completion happens in the public methods OnDownload*().

  void StartImport();
  void OnImageTypeDetected(bool is_iso_image);
  // Calls CreateDiskImage or ImportDiskImage, depending on whether we are
  // creating a new VM from an ISO, or importing a prepared VM image.
  void OnFDPrepared(std::optional<base::ScopedFD> maybe_fd);
  // Callback for the concierge CreateDiskImage/ImportDiskImage calls. The
  // import has just started (unless that failed).
  template <typename ReplyType>
  void OnImportDiskImage(std::optional<ReplyType> reply);
  // Progress updates are sent to OnDiskImageProgress(). After we get a signal
  // that the import is finished successfully, we make one final call to
  // concierge's DiskImageStatus method to get a final resolution.
  void RequestFinalStatus();
  void OnFinalDiskImageStatus(
      std::optional<vm_tools::concierge::DiskImageStatusResponse> response);
  // Finishes the processing of installation. If |failure_reason| has a value,
  // then the import has failed, otherwise it was successful.
  void OnImported(std::optional<FailureReason> failure_reason);

  // End of the install flow!

  void UpdateInstallingState(InstallingState installing_state);
  // Only used on the long-running steps: kDownloadingDlc, kDownloadingImage,
  // kImporting.
  void UpdateProgress(double state_progress);

  // One of InstallFailed() and InstallFinished() will be called at the end of
  // each successfully started installation. These clean up state and log
  // histograms.
  void InstallFailed(FailureReason reason);
  // Callers also need to call the appropriate observer functions indicating
  // success type.
  void InstallFinished();

  // Cancels the image download. The partial download will be deleted.
  void CancelDownload();
  // Calls concierge to cancel the import.
  void CancelImport();
  // Callback for the concierge CancelDiskImageOperation call.
  void OnImportDiskImageCancelled(
      std::optional<vm_tools::concierge::CancelDiskImageResponse> response);
  // Called once cancel is completed, firing the OnCancelFinished() observer
  // event.
  void CancelFinished();

  // Stringify for logging purposes.
  static std::string GetStateName(State state);
  static std::string GetInstallingStateName(InstallingState state);

  GURL GetPluginVmImageDownloadUrl();
  download::DownloadParams GetDownloadParams(const GURL& url);

  void RemoveTemporaryImageIfExists();
  void OnTemporaryImageRemoved(bool success);

  device::mojom::WakeLock* GetWakeLock();

  raw_ptr<Profile> profile_ = nullptr;
  raw_ptr<Observer, DanglingUntriaged> observer_ = nullptr;
  raw_ptr<download::BackgroundDownloadService, DanglingUntriaged>
      download_service_ = nullptr;
  State state_ = State::kIdle;
  InstallingState installing_state_ = InstallingState::kInactive;
  std::string current_download_guid_;
  base::FilePath downloaded_image_;
  // Used to identify our running import with concierge.
  std::string current_import_command_uuid_;
  int64_t expected_image_size_;
  int64_t downloaded_image_size_;
  bool creating_new_vm_ = false;
  double progress_ = 0;
  std::unique_ptr<PluginVmDriveImageDownloadService> drive_download_service_;
  std::unique_ptr<PluginVmLicenseChecker> license_checker_;
  std::unique_ptr<guest_os::GuestOsDlcInstallation> dlc_installation_;
  bool using_drive_download_service_ = false;

  bool skip_license_check_for_testing_ = false;
  // -1 indicates not set
  int64_t free_disk_space_for_testing_ = -1;
  std::optional<base::FilePath> downloaded_image_for_testing_;

  // Keep the system awake during installation.
  mojo::Remote<device::mojom::WakeLock> wake_lock_;

  base::WeakPtrFactory<PluginVmInstaller> weak_ptr_factory_{this};
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_ASH_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_
