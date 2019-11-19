// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"

namespace download {
class DownloadService;
struct CompletionInfo;
}  // namespace download

class Profile;

namespace plugin_vm {

// PluginVmImageManager is responsible for management of PluginVm image
// including downloading this image from url specified by the user policy,
// and importing the downloaded image archive using concierge D-Bus services.
//
// Only one PluginVm image at a time is allowed to be processed.
// Methods StartDownload() and StartImport() should be
// called in this order. Image processing might be interrupted by
// calling the corresponding cancel methods. If one of the methods mentioned is
// called not in the correct order or before the previous state is finished then
// associated fail method will be called by the manager and image processing
// will be interrupted.
class PluginVmImageManager
    : public KeyedService,
      public chromeos::ConciergeClient::DiskImageObserver {
 public:
  // FailureReasons values can be shown to the user. Do not reorder or renumber
  // these values without careful consideration.
  enum class FailureReason {
    LOGIC_ERROR = 0,
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
  };

  // Observer class for the PluginVm image related events.
  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnDownloadStarted() = 0;
    virtual void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                           int64_t content_length,
                                           base::TimeDelta elapsed_time) = 0;
    virtual void OnDownloadCompleted() = 0;
    virtual void OnDownloadCancelled() = 0;
    virtual void OnDownloadFailed(FailureReason reason) = 0;
    virtual void OnImportProgressUpdated(int percent_completed,
                                         base::TimeDelta elapsed_time) = 0;
    virtual void OnImported() = 0;
    virtual void OnImportCancelled() = 0;
    virtual void OnImportFailed(FailureReason reason) = 0;
  };

  explicit PluginVmImageManager(Profile* profile);

  // Returns true if manager is processing PluginVm image at the moment.
  bool IsProcessingImage();
  void StartDownload();
  // Cancels the download of PluginVm image finishing the image processing.
  // Downloaded PluginVm image archive is being deleted.
  void CancelDownload();

  // Proceed with importing (unzipping and registering) of the VM image.
  // Should be called when download of PluginVm image is successfully completed.
  // If called in other cases - importing is not started and
  // OnImported(false /* success */) is called.
  void StartImport();
  // Makes a call to concierge to cancel the import.
  void CancelImport();

  void SetObserver(Observer* observer);
  void RemoveObserver();

  // Called by PluginVmImageDownloadClient, are not supposed to be used by other
  // classes.
  void OnDownloadStarted();
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length);
  void OnDownloadCompleted(const download::CompletionInfo& info);
  void OnDownloadCancelled();
  void OnDownloadFailed(FailureReason reason);

  // ConciergeClient::DiskImageObserver:
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override;

  // Helper function that returns true in case downloaded PluginVm image
  // archive passes hash verification and false otherwise.
  // Public for testing purposes.
  bool VerifyDownload(const std::string& downloaded_archive_hash);

  void SetDownloadServiceForTesting(
      download::DownloadService* download_service);
  void SetDownloadedPluginVmImageArchiveForTesting(
      const base::FilePath& downloaded_plugin_vm_image_archive);
  std::string GetCurrentDownloadGuidForTesting();

 private:
  enum class State {
    NOT_STARTED,
    DOWNLOADING,
    DOWNLOAD_CANCELLED,
    DOWNLOADED,
    IMPORTING,
    IMPORT_CANCELLED,
    CONFIGURED,
    DOWNLOAD_FAILED,
    IMPORT_FAILED,
  };

  Profile* profile_ = nullptr;
  Observer* observer_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  State state_ = State::NOT_STARTED;
  std::string current_download_guid_;
  base::FilePath downloaded_plugin_vm_image_archive_;
  // Used to identify our running import with concierge:
  std::string current_import_command_uuid_;
  // -1 when is not yet determined.
  int64_t downloaded_plugin_vm_image_size_ = -1;
  base::TimeTicks download_start_tick_;
  base::TimeTicks import_start_tick_;

  ~PluginVmImageManager() override;

  // Get string representation of state for logging purposes.
  std::string GetStateName(State state);

  GURL GetPluginVmImageDownloadUrl();
  download::DownloadParams GetDownloadParams(const GURL& url);

  void OnStartDownload(const std::string& download_guid,
                       download::DownloadParams::StartResult start_result);

  // Callback when PluginVm dispatcher is started (together with supporting
  // services such as concierge). This will then make the call to concierge's
  // ImportDiskImage.
  void OnPluginVmDispatcherStarted(bool success);

  // Callback which is called once we know if concierge is available.
  void OnConciergeAvailable(bool success);

  // Ran as a blocking task preparing the FD for the ImportDiskImage call.
  base::Optional<base::ScopedFD> PrepareFD();

  // Callback when the FD is prepared. Makes the call to ImportDiskImage.
  void OnFDPrepared(base::Optional<base::ScopedFD> maybeFd);

  // Callback for the concierge DiskImageImport call.
  void OnImportDiskImage(
      base::Optional<vm_tools::concierge::ImportDiskImageResponse> reply);

  // After we get a signal that the import is finished successfully, we
  // make one final call to concierge's DiskImageStatus method to get a
  // final resolution.
  void RequestFinalStatus();

  // Callback for the final call to concierge's DiskImageStatus to
  // get the final result of the disk import operation. This moves
  // the manager to a finishing state, depending on the result of the
  // query. Called when the signal for the command indicates that we
  // are done with importing.
  void OnFinalDiskImageStatus(
      base::Optional<vm_tools::concierge::DiskImageStatusResponse> reply);

  // Finishes the processing of PluginVm image. If |failure_reason| has a value,
  // then the import has failed, otherwise it was successful.
  void OnImported(base::Optional<FailureReason> failure_reason);

  // Callback for the concierge CancelDiskImageOperation call.
  void OnImportDiskImageCancelled(
      base::Optional<vm_tools::concierge::CancelDiskImageResponse> reply);

  void RemoveTemporaryPluginVmImageArchiveIfExists();
  void OnTemporaryPluginVmImageArchiveRemoved(bool success);

  base::WeakPtrFactory<PluginVmImageManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmImageManager);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_IMAGE_MANAGER_H_
