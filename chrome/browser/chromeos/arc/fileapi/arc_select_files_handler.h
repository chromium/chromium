// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/select_file_dialog_extension.h"
#include "components/arc/mojom/file_system.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace arc {

class ArcSelectFilesHandler;
class SelectFileDialogHolder;

// Exposed for testing.
extern const char kScriptClickOk[];
extern const char kScriptClickCancel[];
extern const char kScriptClickDirectory[];
extern const char kScriptClickFile[];
extern const char kScriptGetElements[];

// Manages multiple ArcSelectFilesHandler instances.
class ArcSelectFilesHandlersManager {
 public:
  explicit ArcSelectFilesHandlersManager(content::BrowserContext* context);
  ~ArcSelectFilesHandlersManager();

  // Delete all handlers and close all SelectFileDialogs.
  void DeleteAllHandlers() { handlers_by_task_id_.clear(); }

  // Handler for FileSystemHost.SelectFiles.
  // Creates a new ArcSelectFilesHandler instance.
  void SelectFiles(const mojom::SelectFilesRequestPtr& request,
                   mojom::FileSystemHost::SelectFilesCallback callback);

  // Handler for FileSystemHost.OnFileSelectorEvent.
  // Routes the request to the right ArcSelectFilesHandler instance.
  void OnFileSelectorEvent(
      mojom::FileSelectorEventPtr event,
      mojom::FileSystemHost::OnFileSelectorEventCallback callback);

  // Handler for FileSystemHost.GetFileSelectorElements.
  // Routes the request to the right ArcSelectFilesHandler instance.
  void GetFileSelectorElements(
      mojom::GetFileSelectorElementsRequestPtr request,
      mojom::FileSystemHost::GetFileSelectorElementsCallback callback);

 private:
  // Helper function for SelectFiles.
  void EraseHandlerAndRunCallback(
      int task_id,
      mojom::FileSystemHost::SelectFilesCallback callback,
      mojom::SelectFilesResultPtr result);

  content::BrowserContext* const context_;

  // Map of Task ID -> ArcSelectFilesHandler.
  std::map<int, std::unique_ptr<ArcSelectFilesHandler>> handlers_by_task_id_;

  base::WeakPtrFactory<ArcSelectFilesHandlersManager> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcSelectFilesHandlersManager);
};

// Manages a single SelectFileDialog instance.
class ArcSelectFilesHandler : public ui::SelectFileDialog::Listener {
 public:
  explicit ArcSelectFilesHandler(content::BrowserContext* context);
  ~ArcSelectFilesHandler() override;

  void SelectFiles(const mojom::SelectFilesRequestPtr& request,
                   mojom::FileSystemHost::SelectFilesCallback callback);

  void OnFileSelectorEvent(
      mojom::FileSelectorEventPtr event,
      mojom::FileSystemHost::OnFileSelectorEventCallback callback);

  void GetFileSelectorElements(
      mojom::GetFileSelectorElementsRequestPtr request,
      mojom::FileSystemHost::GetFileSelectorElementsCallback callback);

  // ui::SelectFileDialog::Listener overrides:
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  friend class ArcSelectFilesHandlerTest;

  void FilesSelectedInternal(const std::vector<base::FilePath>& files,
                             void* params);

  void SetDialogHolderForTesting(
      std::unique_ptr<SelectFileDialogHolder> dialog_holder);

  Profile* const profile_;

  mojom::FileSystemHost::SelectFilesCallback callback_;
  std::unique_ptr<SelectFileDialogHolder> dialog_holder_;

  DISALLOW_COPY_AND_ASSIGN(ArcSelectFilesHandler);
};

// Wrapper for SelectFileDialogExtension.
// Since it is not easy to create a mock class for SelectFileDialogExtension,
// this class is replaced with a mock class instead in unit tests.
class SelectFileDialogHolder {
 public:
  explicit SelectFileDialogHolder(ui::SelectFileDialog::Listener* listener);
  virtual ~SelectFileDialogHolder();

  // Obtains the owner window from |task_id| and opens the select file dialog
  // with it. Returns false if the owner window is not found.
  virtual bool SelectFile(ui::SelectFileDialog::Type type,
                          const base::FilePath& default_path,
                          const ui::SelectFileDialog::FileTypeInfo* file_types,
                          int task_id,
                          bool show_android_picker_apps);

  virtual void ExecuteJavaScript(
      const std::string& script,
      content::RenderFrameHost::JavaScriptResultCallback callback);

 private:
  scoped_refptr<SelectFileDialogExtension> select_file_dialog_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
