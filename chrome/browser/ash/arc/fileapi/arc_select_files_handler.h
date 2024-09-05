// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_

#include <vector>

#include "ash/components/arc/mojom/file_system.mojom.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/select_file_dialog_extension/select_file_dialog_extension.h"
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
// Script for clicking OK button on the selector.
inline constexpr char kScriptClickOk[] =
    "(function() { document.querySelector('#ok-button').click(); })();";

// Script for clicking Cancel button on the selector.
inline constexpr char kScriptClickCancel[] =
    "(function() { document.querySelector('#cancel-button').click(); })();";

// Script for clicking a directory element in the left pane of the selector.
// %s should be replaced by the target directory name wrapped by double-quotes.
inline constexpr char kScriptClickDirectory[] =
    "(function() {"
    "  var dirs = document.querySelectorAll('#directory-tree xf-tree-item');"
    "  Array.from(dirs).filter(a => a.getAttribute('label') === %s)[0].click();"
    "})();";

// Script for clicking a file element in the right pane of the selector.
// %s should be replaced by the target file name wrapped by double-quotes.
inline constexpr char kScriptClickFile[] =
    "(function() {"
    "  var evt = document.createEvent('MouseEvents');"
    "  evt.initMouseEvent('mousedown', true, false);"
    "  var files = document.querySelectorAll('#file-list .file');"
    "  Array.from(files).filter(a => a.getAttribute('file-name') === %s)[0]"
    "      .dispatchEvent(evt);"
    "})();";

// Script for querying UI elements (directories and files) shown on the
// selector.
inline constexpr char kScriptGetElements[] =
    "(function() {"
    "  var dirs = document.querySelectorAll('#directory-tree xf-tree-item');"
    "  var files = document.querySelectorAll('#file-list .file');"
    "  return {dirNames: Array.from(dirs, a => a.getAttribute('label')),"
    "          fileNames: Array.from(files, a => a.getAttribute('file-name'))};"
    "})();";

// Manages multiple ArcSelectFilesHandler instances.
class ArcSelectFilesHandlersManager {
 public:
  explicit ArcSelectFilesHandlersManager(content::BrowserContext* context);

  ArcSelectFilesHandlersManager(const ArcSelectFilesHandlersManager&) = delete;
  ArcSelectFilesHandlersManager& operator=(
      const ArcSelectFilesHandlersManager&) = delete;

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

  const raw_ptr<content::BrowserContext> context_;

  // Map of Task ID -> ArcSelectFilesHandler.
  std::map<int, std::unique_ptr<ArcSelectFilesHandler>> handlers_by_task_id_;

  base::WeakPtrFactory<ArcSelectFilesHandlersManager> weak_ptr_factory_{this};
};

// Manages a single SelectFileDialog instance.
class ArcSelectFilesHandler : public ui::SelectFileDialog::Listener {
 public:
  explicit ArcSelectFilesHandler(content::BrowserContext* context);

  ArcSelectFilesHandler(const ArcSelectFilesHandler&) = delete;
  ArcSelectFilesHandler& operator=(const ArcSelectFilesHandler&) = delete;

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
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

 private:
  friend class ArcSelectFilesHandlerTest;

  void FilesSelectedInternal(const std::vector<ui::SelectedFileInfo>& files);

  void SetDialogHolderForTesting(
      std::unique_ptr<SelectFileDialogHolder> dialog_holder);

  const raw_ptr<Profile> profile_;

  mojom::FileSystemHost::SelectFilesCallback callback_;
  std::unique_ptr<SelectFileDialogHolder> dialog_holder_;
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
                          const std::string& search_query,
                          bool show_android_picker_apps,
                          bool use_media_store_filter);

  virtual void ExecuteJavaScript(
      const std::string& script,
      content::RenderFrameHost::JavaScriptResultCallback callback);

 private:
  scoped_refptr<SelectFileDialogExtension> select_file_dialog_;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_SELECT_FILES_HANDLER_H_
