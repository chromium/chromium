// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_

#include <memory>
#include <queue>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/native_widget_types.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace policy {

// The type of policy notification.
enum class NotificationType {
  kError,
  kWarning,
};

// The policy notification button index.
enum NotificationButton {
  CANCEL = 0,
  OK,
};

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

  // Show DLP block UI. If `task_id` is set, the corresponding IOTask will be
  // updated with the blocked files. Otherwise a desktop notification will be
  // shown.
  virtual void ShowDlpBlockedFiles(
      absl::optional<file_manager::io_task::IOTaskId> task_id,
      std::vector<base::FilePath> blocked_files,
      dlp::FileAction action);

  // Shows DLP Warning UI. If `task_id` is set, the corresponding IOTask will be
  // paused. Otherwise a desktop notification will be shown.
  // Virtual to allow overrides in tests.
  virtual void ShowDlpWarning(
      OnDlpRestrictionCheckedCallback callback,
      absl::optional<file_manager::io_task::IOTaskId> task_id,
      std::vector<base::FilePath> warning_files,
      const DlpFileDestination& destination,
      dlp::FileAction action);

  // Shows a Files Policy warning or error desktop notification with
  // `notification_id` based on `status`.
  void ShowsFilesPolicyNotification(
      const std::string& notification_id,
      const file_manager::io_task::ProgressStatus& status);

  // Shows a policy dialog of type `type` for task identified by `task_id`.
  // Used for copy and move operations.
  void ShowDialog(file_manager::io_task::IOTaskId task_id,
                  FilesDialogType type);

  // Returns whether IO task is being tracked.
  bool HasIOTask(file_manager::io_task::IOTaskId task_id) const;

  // Runs warning callback for the corresponding IOTask with should_proceed set
  // to true.
  void OnIOTaskResumed(file_manager::io_task::IOTaskId task_id);

  std::map<DlpConfidentialFile, Policy> GetIOTaskBlockedFilesForTesting(
      file_manager::io_task::IOTaskId task_id) const;

  // Used in tests to set the test task runner.
  void SetTaskRunnerForTesting(scoped_refptr<base::SequencedTaskRunner>);

 protected:
  // The number of notifications shown so far. Used to calculate a unique
  // notification ID. Only applies to non IOTasks operations (upload, download,
  // etc.) as notifications for IOTasks are shown based on the task state from
  // the SystemNotificationManager.
  size_t notification_count_ = 0;

 private:
  // Holds all information related to file task warning. Any extra information
  // needed for custom messaging should be added here.
  struct WarningInfo {
    WarningInfo() = delete;
    WarningInfo(std::vector<base::FilePath> files_paths,
                Policy warning_reason,
                OnDlpRestrictionCheckedCallback warning_callback,
                OnDlpRestrictionCheckedCallback dialog_callback);
    WarningInfo(std::vector<DlpConfidentialFile> files,
                Policy warning_reason,
                OnDlpRestrictionCheckedCallback warning_callback,
                OnDlpRestrictionCheckedCallback dialog_callback);
    WarningInfo(WarningInfo&& other);
    ~WarningInfo();

    // Warning files.
    std::vector<DlpConfidentialFile> files;
    // Warning reason. There should be only one policy per warning as mixed
    // warnings aren't supported.
    Policy warning_reason;
    // Warning callback.
    OnDlpRestrictionCheckedCallback warning_callback;
    // Invoked by clicking on dialog's buttons. Wrapper around `callback` as it
    // performs additional actions before running `callback` with the same
    // `should_proceed` parameter.
    OnDlpRestrictionCheckedCallback dialog_callback;
  };

  // Holds needed information for each tracked file task.
  struct FileTaskInfo {
    explicit FileTaskInfo(dlp::FileAction action);
    FileTaskInfo(FileTaskInfo&& other);
    ~FileTaskInfo();

    // Should have value only if there's warning.
    absl::optional<WarningInfo> warning_info;
    // A map of all files blocked to be transferred and the block reason for
    // each.
    std::map<DlpConfidentialFile, Policy> blocked_files;
    // The action that's restricted.
    dlp::FileAction action;
    // The destination of the action. Optional.
    // TODO(b/285568353): Remove this.
    absl::optional<DlpFileDestination> destination;
  };

  // Callback to show the dialog. Invoked with a Files App window when
  // successfully opened, or null if opening the Files App times out.
  using ShowDialogCallback = base::OnceCallback<void(gfx::NativeWindow)>;

  // Holds information for showing a Files Policy dialog.
  struct DialogInfo {
    DialogInfo() = delete;
    DialogInfo(ShowDialogCallback callback,
               file_manager::io_task::IOTaskId task_id,
               base::OnceClosure timeout_callback);
    DialogInfo(ShowDialogCallback callback,
               std::string notification_id,
               base::OnceClosure timeout_callback);
    ~DialogInfo();

    // Id of the task for which dialog is being shown. Used for Copy and Move
    // IOTasks.
    absl::optional<file_manager::io_task::IOTaskId> task_id;
    // Id of the notification for which dialog is being shown. Used for non IO
    // tasks.
    absl::optional<std::string> notification_id;
    // Callback to show the dialog.
    ShowDialogCallback dialog_callback;
    // Callback to stop waiting for the Files app.
    base::OnceClosure timeout_callback;
    base::TimeTicks created_at;
    base::OneShotTimer timeout_timer;
  };

  // Click handler for Data Leak Prevention or Enterprise Connectors policy
  // warning notifications.
  void HandleFilesPolicyWarningNotificationClick(
      file_manager::io_task::IOTaskId task_id,
      std::string notification_id,
      absl::optional<int> button_index);

  // Click handler for Data Leak Prevention or Enterprise Connectors policy
  // error notifications.
  void HandleFilesPolicyErrorNotificationClick(
      file_manager::io_task::IOTaskId task_id,
      std::string notification_id,
      absl::optional<int> button_index);

  // Click handler for DLP warning notifications. Used for non IO tasks.
  void HandleDlpWarningNotificationClick(
      std::string notification_id,
      OnDlpRestrictionCheckedCallback callback,
      std::vector<DlpConfidentialFile> files,
      DlpFileDestination destination,
      dlp::FileAction action,
      absl::optional<int> button_index);

  // Click handler for DLP error notifications. Used for non IO tasks.
  void HandleDlpErrorNotificationClick(std::string notification_id,
                                       std::vector<DlpConfidentialFile> files,
                                       dlp::FileAction action,
                                       absl::optional<int> button_index);

  // Shows a FilesPolicyDialog of `type` for task with `task_id`.
  void ShowDialogForIOTask(file_manager::io_task::IOTaskId task_id,
                           FilesDialogType type,
                           gfx::NativeWindow modal_parent);

  // Shows a FilesPolicyDialog of `type` for non-IO task associated with
  // `notification_id`.
  void ShowDialogForNonIOTask(std::string notification_id,
                              FilesDialogType type,
                              gfx::NativeWindow modal_parent);

  // Shows a FilesPolicyDialog of `type` based on `info`.
  void ShowFilesPolicyDialog(FileTaskInfo& info,
                             FilesDialogType type,
                             gfx::NativeWindow modal_parent);

  // Starts tracking IO task with `task_id`.
  void AddIOTask(file_manager::io_task::IOTaskId task_id,
                 dlp::FileAction action);

  // Launches the Files App in default directory and appends `dialog_info` to
  // the queue of pending dialogs in order to show the dialog over it.
  void LaunchFilesApp(std::unique_ptr<DialogInfo> dialog_info);

  // BrowserListObserver overrides:
  // Called when opening a new Files App window to use as the modal parent for a
  // FilesPolicyDialog.
  void OnBrowserSetLastActive(Browser* browser) override;

  // file_manager::io_task::IOTaskController::Observer overrides:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override;

  // Returns whether IO task has any blocked file.
  bool HasBlockedFiles(file_manager::io_task::IOTaskId task_id) const;

  // Returns whether IO task has a warning.
  bool HasWarning(file_manager::io_task::IOTaskId task_id) const;

  // Returns whether non IO task is being tracked.
  bool HasNonIOTask(const std::string notification_id) const;

  // Returns whether non IO task has any blocked file.
  bool HasBlockedFiles(const std::string notification_id) const;

  // Returns whether non IO task has a warning.
  bool HasWarning(const std::string notification_id) const;

  // Called when the user clicks on one of the warning dialog's buttons.
  // Resumes/cancels the task with `task_id` based on the value of
  // `should_proceed`. Used for Copy and Move IOTasks.
  void OnIOTaskWarningDialogClicked(file_manager::io_task::IOTaskId task_id,
                                    Policy warning_reason,
                                    bool should_proceed);

  // Called when the user clicks on one of the warning dialog's buttons.
  // associated with `notification_id`. Resumes/cancels the operation based on
  // the value of `should_proceed`.
  void OnNonIOTaskWarningDialogClicked(const std::string& notification_id,
                                       Policy warning_reason,
                                       bool should_proceed);

  // Opens DLP Learn more link and closes the notification having
  // `notification_id`.
  void OnLearnMoreButtonClicked(const std::string& notification_id,
                                absl::optional<int> button_index);

  // Calls the IOTaskController to resume the task with `task_id`.
  void Resume(file_manager::io_task::IOTaskId task_id);

  // Calls the IOTaskController to cancel the task with `task_id`.
  void Cancel(file_manager::io_task::IOTaskId task_id);

  // KeyedService overrides:
  void Shutdown() override;

  // Shows DLP block desktop notification.
  void ShowDlpBlockNotification(std::vector<base::FilePath> blocked_files,
                                dlp::FileAction action);

  // Shows DLP warning desktop notification.
  void ShowDlpWarningNotification(OnDlpRestrictionCheckedCallback callback,
                                  std::vector<base::FilePath> warning_files,
                                  const DlpFileDestination& destination,
                                  dlp::FileAction action);

  // Pauses IO task due to `warning_reason`.
  void PauseIOTask(file_manager::io_task::IOTaskId task_id,
                   OnDlpRestrictionCheckedCallback callback,
                   std::vector<base::FilePath> warning_files,
                   dlp::FileAction action,
                   Policy warning_reason);

  // Starts a timer when `info` is created.
  void StartTimer(DialogInfo* info, base::OnceClosure on_timeout_callback);

  // Called after opening the Files App times out.
  // Stops waiting for the app and shows a dialog for `task_id` without a modal
  // parent (i.e. as a system modal).
  void OnIOTaskTimedOut(file_manager::io_task::IOTaskId task_id);

  // Called after opening the Files App times out.
  // Stops waiting for the app and shows a dialog for `notification_id` without
  // a modal parent (i.e. as a system modal).
  void OnNonIOTaskTimedOut(std::string notification_id);

  // Helper method that pops the oldest entry from `pending_dialogs_` and
  // creates a dialog with with `modal_parent`. No-op if the list is empty.
  void ShowPendingDialog(gfx::NativeWindow modal_parent);

  // Callback to show a policy dialog after waiting to open a Files App window.
  base::OnceCallback<void(gfx::NativeWindow)> pending_callback_;

  // Context for which the FPNM is created.
  raw_ptr<content::BrowserContext, ExperimentalAsh> context_;

  // A map from tracked IO tasks ids to their info.
  std::map<file_manager::io_task::IOTaskId, FileTaskInfo> io_tasks_;

  // A map from notification ids to related task info for non IO operations.
  std::map<std::string, FileTaskInfo> non_io_tasks_;

  // Callbacks to show a policy dialog after waiting to open a Files App window.
  std::queue<std::unique_ptr<DialogInfo>> pending_dialogs_;

  // Used to fallack to system modal if opening the Files App times out.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<FilesPolicyNotificationManager> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_
