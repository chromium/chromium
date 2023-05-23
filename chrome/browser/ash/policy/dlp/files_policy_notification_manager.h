// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {

// FilesPolicyNotificationManager is responsible for showing block and warning
// notifications/dialogs for files because of DLP and enterprise connectors
// policies.
class FilesPolicyNotificationManager
    : public KeyedService,
      public BrowserListObserver,
      public file_manager::io_task::IOTaskController::Observer {
 public:
  explicit FilesPolicyNotificationManager(content::BrowserContext* context);

  FilesPolicyNotificationManager(const FilesPolicyNotificationManager&) =
      delete;
  FilesPolicyNotificationManager& operator=(
      const FilesPolicyNotificationManager&) = delete;

  ~FilesPolicyNotificationManager() override;

  // Shows a policy dialog of type `type` and `policy` for task identified by
  // `task_id`. Used for copy and move operations.
  void ShowDialog(file_manager::io_task::IOTaskId task_id,
                  FilesDialogType type,
                  absl::optional<policy::Policy> policy);

  // Returns whether IO task is being tracked.
  bool HasIOTask(file_manager::io_task::IOTaskId task_id) const;

 private:
  // Holds all information related to IO task warning. Any extra information
  // needed for custom messaging should be added here.
  struct WarningInfo {
    WarningInfo() = delete;
    WarningInfo(std::vector<base::FilePath> files_paths, Policy warning_reason);
    WarningInfo(WarningInfo&& other);
    ~WarningInfo();

    // Warning files.
    std::vector<DlpConfidentialFile> files;
    // Warning reason. There should be only one policy per warning as mixed
    // warnings aren't supported.
    Policy warning_reason;
  };

  // Holds needed information for each tracked IO task.
  struct IOTaskInfo {
    IOTaskInfo();
    IOTaskInfo(IOTaskInfo&& other);
    ~IOTaskInfo();

    // Should have value only if there's warning.
    absl::optional<WarningInfo> warning_info;
    // A map of all files blocked to be transferred and the block reason for
    // each.
    std::map<DlpConfidentialFile, Policy> blocked_files;
  };

  // Shows a FilesPolicyDialog.
  void ShowFilesPolicyDialog(FilesDialogType type,
                             absl::optional<policy::Policy> policy,
                             gfx::NativeWindow modal_parent);

  // Starts tracking IO task with `task_id`.
  void AddIOTask(file_manager::io_task::IOTaskId task_id);

  // BrowserListObserver overrides:
  // Called when opening a new Files App window to use as the modal parent for a
  // FilesPolicyDialog.
  void OnBrowserSetLastActive(Browser* browser) override;

  // file_manager::io_task::IOTaskController::Observer overrides:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override;

  // Returns IO task warning files due to `warning_reason`.
  std::vector<DlpConfidentialFile> GetWarningFiles(
      file_manager::io_task::IOTaskId task_id,
      Policy warning_reason) const;

  // Returns whether IO task has any blocked file.
  bool HasBlockedFiles(file_manager::io_task::IOTaskId task_id) const;

  // Callback to show a policy dialog after waiting to open a Files App window.
  base::OnceCallback<void(gfx::NativeWindow)> pending_callback_;

  // Context for which the FPNM is created.
  content::BrowserContext* context_;

  // A map from tracked IO tasks ids to their info.
  std::map<file_manager::io_task::IOTaskId, IOTaskInfo> io_tasks_;

  // Observes IOTaskController to get updates about IO tasks.
  base::ScopedObservation<file_manager::io_task::IOTaskController,
                          file_manager::io_task::IOTaskController::Observer>
      io_tasks_observation_{this};

  base::WeakPtrFactory<FilesPolicyNotificationManager> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_
