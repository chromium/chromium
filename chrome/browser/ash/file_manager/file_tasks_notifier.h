// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_NOTIFIER_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_NOTIFIER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/file_manager/file_tasks_observer.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/drive/file_errors.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace ui {
struct SelectedFileInfo;
}

namespace file_manager {
namespace file_tasks {

class FileTasksNotifier : public KeyedService,
                          public download::AllDownloadItemNotifier::Observer {
 public:
  enum class FileAvailability {
    // File exists and is accessible.
    kOk,

    // The file exists but is currently unavailable; e.g. a Drive file while
    // offline
    kTemporarilyUnavailable,

    // The current state is unknown; e.g. crostini isn't running
    kUnknown,

    // The file is known to be deleted.
    kGone,
  };

  explicit FileTasksNotifier(Profile* profile);

  FileTasksNotifier(const FileTasksNotifier&) = delete;
  FileTasksNotifier& operator=(const FileTasksNotifier&) = delete;

  ~FileTasksNotifier() override;

  void AddObserver(FileTasksObserver*);
  void RemoveObserver(FileTasksObserver*);

  void QueryFileAvailability(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(std::vector<FileAvailability>)>);

  void NotifyFileTasks(const std::vector<storage::FileSystemURL>& file_urls);

  void NotifyFileDialogSelection(const std::vector<ui::SelectedFileInfo>& files,
                                 bool for_open);

  // download::AllDownloadItemNotifier::Observer:
  void OnDownloadUpdated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

 private:
  struct PendingFileAvailabilityTask;
  void NotifyObservers(const std::vector<base::FilePath>& paths,
                       FileTasksObserver::OpenType open_type);

  void GetFileAvailability(PendingFileAvailabilityTask task);
  static void ForwardQueryResult(PendingFileAvailabilityTask task, bool exists);

  static void ForwardDriveFsQueryResult(
      PendingFileAvailabilityTask task,
      bool is_offline,
      drive::FileError error,
      drivefs::mojom::FileMetadataPtr metadata);

  // Virtual for stubbing out in tests.
  virtual drivefs::mojom::DriveFs* GetDriveFsInterface();
  virtual bool GetRelativeDrivePath(const base::FilePath& path,
                                    base::FilePath* drive_relative_path);
  virtual bool IsOffline();

  const raw_ptr<Profile> profile_;
  download::AllDownloadItemNotifier download_notifier_;
  base::ObserverList<FileTasksObserver> observers_;
};

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_NOTIFIER_H_
