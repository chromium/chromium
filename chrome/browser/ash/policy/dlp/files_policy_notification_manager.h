// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_

#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/file_manager/io_task.h"
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
class FilesPolicyNotificationManager : public KeyedService,
                                       public BrowserListObserver {
 public:
  explicit FilesPolicyNotificationManager(content::BrowserContext* context);

  FilesPolicyNotificationManager(const FilesPolicyNotificationManager&) =
      delete;
  FilesPolicyNotificationManager& operator=(
      const FilesPolicyNotificationManager&) = delete;

  ~FilesPolicyNotificationManager() override;

  // Shows a policy dialog of type `type` for task identified by `task_id`. Used
  // for copy and move operations.
  void ShowDialog(file_manager::io_task::IOTaskId task_id,
                  FilesDialogType type);

 private:
  // Shows a FilesPolicyDialog.
  void ShowFilesPolicyDialog(
      OnDlpRestrictionCheckedCallback callback,
      const std::vector<DlpConfidentialFile>& confidential_files,
      const DlpFileDestination& destination,
      DlpFilesController::FileAction action,
      gfx::NativeWindow modal_parent);

  // BrowserListObserver overrides:
  // Called when opening a new Files App window to use as the modal parent for a
  // FilesPolicyDialog.
  void OnBrowserSetLastActive(Browser* browser) override;

  // Callback to show a policy dialog after waiting to open a Files App window.
  base::OnceCallback<void(gfx::NativeWindow)> pending_callback_;

  // Context for which the FPNM is created.
  content::BrowserContext* context_;

  base::WeakPtrFactory<FilesPolicyNotificationManager> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_FILES_POLICY_NOTIFICATION_MANAGER_H_
