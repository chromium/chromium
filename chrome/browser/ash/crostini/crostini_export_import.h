// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/ash/crostini/crostini_export_import_notification_controller.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/guest_os/guest_id.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path.h"
#include "components/keyed_service/core/keyed_service.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace content {
class WebContents;
}

namespace crostini {

enum class ExportContainerResult;
enum class ImportContainerResult;

enum class ExportImportType {
  EXPORT,
  IMPORT,
  EXPORT_DISK_IMAGE,
  IMPORT_DISK_IMAGE
};

// ExportContainerResult is used for UMA, if you update this make sure to update
// CrostiniExportContainerResult in enums.xml
enum class ExportContainerResult {
  kSuccess = 0,
  kFailed = 1,
  kFailedVmStopped = 2,
  kFailedVmStarted = 3,
  kMaxValue = kFailedVmStarted,
};

// ImportContainerResult is used for UMA, if you update this make sure to update
// CrostiniImportContainerResult in enums.xml
enum class ImportContainerResult {
  kSuccess = 0,
  kFailed = 1,
  kFailedVmStopped = 2,
  kFailedVmStarted = 3,
  kFailedArchitecture = 4,
  kFailedSpace = 5,
  kMaxValue = kFailedSpace,
};

// CrostiniExportImport is a keyed profile service to manage exporting and
// importing containers with crostini.  It manages a file dialog for selecting
// files and a notification to show the progress of export/import.
//
// TODO(crbug.com/41441501): Ensure we have enough free space before doing
// backup or restore.
class CrostiniExportImport : public KeyedService,
                             public ui::SelectFileDialog::Listener,
                             public crostini::ExportContainerProgressObserver,
                             public crostini::ImportContainerProgressObserver,
                             public crostini::DiskImageProgressObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called immediately before operation begins with |in_progress|=true, and
    // again immediately after the operation completes with |in_progress|=false.
    virtual void OnCrostiniExportImportOperationStatusChanged(
        bool in_progress) = 0;
  };

  using OnceTrackerFactory = base::OnceCallback<std::unique_ptr<
      CrostiniExportImportStatusTracker>(ExportImportType, base::FilePath)>;

  struct OperationData {
    OperationData(ExportImportType type,
                  guest_os::GuestId id,
                  OnceTrackerFactory factory);
    ~OperationData();

    ExportImportType type;
    guest_os::GuestId container_id;
    OnceTrackerFactory tracker_factory;
  };

  explicit CrostiniExportImport(Profile* profile);

  CrostiniExportImport(const CrostiniExportImport&) = delete;
  CrostiniExportImport& operator=(const CrostiniExportImport&) = delete;

  ~CrostiniExportImport() override;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // KeyedService:
  void Shutdown() override;

  // Export the |container_id| showing FileDialog.
  void ExportContainer(guest_os::GuestId container_id,
                       content::WebContents* web_contents);
  // Import the |container_id| showing FileDialog.
  void ImportContainer(guest_os::GuestId container_id,
                       content::WebContents* web_contents);

  // Export |container_id| to |path| and invoke |callback| when complete.
  void ExportContainer(guest_os::GuestId container_id,
                       base::FilePath path,
                       CrostiniManager::CrostiniResultCallback callback);

  // Import |container_id| from |path| and invoke |callback| when complete.
  void ImportContainer(guest_os::GuestId container_id,
                       base::FilePath path,
                       CrostiniManager::CrostiniResultCallback callback);

  // Create a new container with |container_id| from |path| and invoke
  // |callback| when complete.
  void CreateContainerFromImport(
      guest_os::GuestId container_id,
      base::FilePath path,
      CrostiniManager::CrostiniResultCallback callback);

  // Export |container_id| showing FileDialog, and using |tracker_factory| for
  // status tracking.
  void ExportContainer(guest_os::GuestId container_id,
                       content::WebContents* web_contents,
                       OnceTrackerFactory tracker_factory);
  // Import |container_id| showing FileDialog, and using |tracker_factory| for
  // status tracking.
  void ImportContainer(guest_os::GuestId container_id,
                       content::WebContents* web_contents,
                       OnceTrackerFactory tracker_factory);

  // Export |container| to |path| and invoke |tracker_factory| to create a
  // tracker for this operation.
  void ExportContainer(guest_os::GuestId container_id,
                       base::FilePath path,
                       OnceTrackerFactory tracker_factory);
  // Import |container| from |path| and invoke |tracker_factory| to create a
  // tracker for this operation.
  void ImportContainer(guest_os::GuestId container_id,
                       base::FilePath path,
                       OnceTrackerFactory tracker_factory);

  // Cancel currently running export/import.
  void CancelOperation(ExportImportType type, guest_os::GuestId id);

  // Whether an export or import is currently in progress.
  bool GetExportImportOperationStatus() const;

  // Returns the default location to export the container to.
  base::FilePath GetDefaultBackupPath() const;

  base::WeakPtr<CrostiniExportImportNotificationController>
  GetNotificationControllerForTesting(guest_os::GuestId container_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestDeprecatedExportSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestExportCustomVmContainerSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestExportDoneBeforeCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportCustomVmContainerSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportDoneBeforeCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportFailArchitecture);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportFailSpace);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestExportDiskImageSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportDiskImageFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestExportDiskImageCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportDiskImageSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportDiskImageFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportDiskImageCancelled);

  void FillOperationData(ExportImportType type,
                         guest_os::GuestId id,
                         OnceTrackerFactory cb);
  void FillOperationData(ExportImportType type, guest_os::GuestId id);
  void FillOperationData(ExportImportType type);

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  void Start(base::FilePath path,
             bool create_new_container,
             CrostiniManager::CrostiniResultCallback callback);

  // Restart VM with LXD if required and share the file path with VM.
  void EnsureLxdStartedThenSharePath(
      const guest_os::GuestId& container_id,
      const base::FilePath& path,
      bool persist,
      bool create_new_container,
      guest_os::GuestOsSharePath::SharePathCallback callback);

  // Share the file path with VM after VM has been restarted.
  void SharePath(const std::string& vm_name,
                 const base::FilePath& path,
                 guest_os::GuestOsSharePath::SharePathCallback callback,
                 crostini::CrostiniResult result);

  // crostini::ExportContainerProgressObserver implementation.
  void OnExportContainerProgress(const guest_os::GuestId& container_id,
                                 const StreamingExportStatus& status) override;

  // crostini::DiskImageProgressObserver implementation.
  void OnDiskImageProgress(const guest_os::GuestId& container_id,
                           DiskImageProgressStatus status,
                           int progress) override;

  // crostini::ImportContainerProgressObserver implementation.
  void OnImportContainerProgress(const guest_os::GuestId& container_id,
                                 crostini::ImportContainerProgressStatus status,
                                 int progress_percent,
                                 uint64_t progress_speed,
                                 const std::string& architecture_device,
                                 const std::string& architecture_container,
                                 uint64_t available_space,
                                 uint64_t minimum_required_space) override;

  void ExportDiskImage(const guest_os::GuestId& container_id,
                       const base::FilePath& path,
                       CrostiniManager::CrostiniResultCallback callback,
                       CrostiniResult result);

  void ImportDiskImage(const guest_os::GuestId& container_id,
                       const base::FilePath& path,
                       CrostiniManager::CrostiniResultCallback callback,
                       CrostiniResult result);

  void AfterDiskImageOperation(const guest_os::GuestId& container_id,
                               CrostiniManager::CrostiniResultCallback callback,
                               CrostiniResult result);

  void ExportAfterSharing(const guest_os::GuestId& container_id,
                          const base::FilePath& path,
                          CrostiniManager::CrostiniResultCallback callback,
                          const base::FilePath& container_path,
                          bool result,
                          const std::string& failure_reason);
  void OnExportComplete(const base::Time& start,
                        const guest_os::GuestId& container_id,
                        CrostiniManager::CrostiniResultCallback callback,
                        CrostiniResult result,
                        uint64_t container_size,
                        uint64_t compressed_size);

  void ImportAfterSharing(const guest_os::GuestId& container_id,
                          const base::FilePath& path,
                          CrostiniManager::CrostiniResultCallback callback,
                          const base::FilePath& container_path,
                          bool result,
                          const std::string& failure_reason);
  void OnImportComplete(const base::Time& start,
                        const guest_os::GuestId& container_id,
                        CrostiniManager::CrostiniResultCallback callback,
                        CrostiniResult result);

  void OpenFileDialog(content::WebContents* web_contents);

  std::string GetUniqueNotificationId();

  using TrackerMap =
      std::map<guest_os::GuestId,
               std::unique_ptr<CrostiniExportImportStatusTracker>>;

  std::unique_ptr<CrostiniExportImportStatusTracker> RemoveTracker(
      TrackerMap::iterator it);

  raw_ptr<Profile> profile_;
  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;
  TrackerMap status_trackers_;
  std::unique_ptr<OperationData> operation_data_;
  // Trackers must have unique-per-profile identifiers.
  // A non-static member on a profile-keyed-service will suffice.
  int next_status_tracker_id_ = 0;
  base::ObserverList<Observer> observers_;
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<CrostiniExportImport> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_EXPORT_IMPORT_H_
