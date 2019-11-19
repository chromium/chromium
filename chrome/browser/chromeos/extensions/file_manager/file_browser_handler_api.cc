// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The file contains the implementation of
// fileBrowserHandlerInternal.selectFile extension function.
// When invoked, the function does the following:
//  - Verifies that the extension function was invoked as a result of user
//    gesture.
//  - Display 'save as' dialog using FileSelectorImpl which waits for the user
//    feedback.
//  - Once the user selects the file path (or cancels the selection),
//    FileSelectorImpl notifies FileBrowserHandlerInternalSelectFileFunction of
//    the selection result by calling FileHandlerSelectFile::OnFilePathSelected.
//  - If the selection was canceled,
//    FileBrowserHandlerInternalSelectFileFunction returns reporting failure.
//  - If the file path was selected, the function opens external file system
//    needed to create FileEntry object for the selected path
//    (opening file system will create file system name and root url for the
//    caller's external file system).
//  - The function grants permissions needed to read/write/create file under the
//    selected path. To grant permissions to the caller, caller's extension ID
//    has to be allowed to access the files virtual path (e.g. /Downloads/foo)
//    in ExternalFileSystemBackend. Additionally, the callers render
//    process ID has to be granted read, write and create permissions for the
//    selected file's full filesystem path (e.g.
//    /home/chronos/user/Downloads/foo) in ChildProcessSecurityPolicy.
//  - After the required file access permissions are granted, result object is
//    created and returned back.

#include "chrome/browser/chromeos/extensions/file_manager/file_browser_handler_api.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/api/file_browser_handler_internal.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_info.h"
#include "storage/common/file_system/file_system_util.h"
#include "ui/shell_dialogs/select_file_dialog.h"

using content::BrowserThread;
using extensions::api::file_browser_handler_internal::FileEntryInfo;
using file_manager::FileSelector;
using file_manager::FileSelectorFactory;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

namespace {

const char kNoUserGestureError[] =
    "This method can only be called in response to user gesture, such as a "
    "mouse click or key press.";

// Converts file extensions to a ui::SelectFileDialog::FileTypeInfo.
ui::SelectFileDialog::FileTypeInfo ConvertExtensionsToFileTypeInfo(
    const std::vector<std::string>& extensions) {
  ui::SelectFileDialog::FileTypeInfo file_type_info;

  for (size_t i = 0; i < extensions.size(); ++i) {
    base::FilePath::StringType allowed_extension =
        base::FilePath::FromUTF8Unsafe(extensions[i]).value();

    // FileTypeInfo takes a nested vector like [["htm", "html"], ["txt"]] to
    // group equivalent extensions, but we don't use this feature here.
    std::vector<base::FilePath::StringType> inner_vector;
    inner_vector.push_back(allowed_extension);
    file_type_info.extensions.push_back(inner_vector);
  }

  return file_type_info;
}

// File selector implementation.
// When |SelectFile| is invoked, it will show save as dialog and listen for user
// action. When user selects the file (or closes the dialog), the function's
// |OnFilePathSelected| method will be called with the result.
// SelectFile should be called only once, because the class instance takes
// ownership of itself after the first call. It will delete itself after the
// extension function is notified of file selection result.
// Since the extension function object is ref counted, FileSelectorImpl holds
// a reference to it to ensure that the extension function doesn't go away while
// waiting for user action. The reference is released after the function is
// notified of the selection result.
class FileSelectorImpl : public FileSelector,
                         public ui::SelectFileDialog::Listener {
 public:
  FileSelectorImpl();
  ~FileSelectorImpl() override;

 protected:
  // file_manager::FileSelectr overrides.
  // Shows save as dialog with suggested name in window bound to |browser|.
  // |allowed_extensions| specifies the file extensions allowed to be shown,
  // and selected. Extensions should not include '.'.
  //
  // After this method is called, the selector implementation should not be
  // deleted by the caller. It will delete itself after it receives response
  // from SelectFielDialog.
  void SelectFile(
      const base::FilePath& suggested_name,
      const std::vector<std::string>& allowed_extensions,
      Browser* browser,
      FileBrowserHandlerInternalSelectFileFunction* function) override;

  // ui::SelectFileDialog::Listener overrides.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  // Initiates and shows 'save as' dialog which will be used to prompt user to
  // select a file path. The initial selected file name in the dialog will be
  // set to |suggested_name|. The dialog will be bound to the tab active in
  // |browser|.
  // |allowed_extensions| specifies the file extensions allowed to be shown,
  // and selected. Extensions should not include '.'.
  //
  // Returns boolean indicating whether the dialog has been successfully shown
  // to the user.
  bool StartSelectFile(const base::FilePath& suggested_name,
                       const std::vector<std::string>& allowed_extensions,
                       Browser* browser);

  // Reacts to the user action reported by the dialog and notifies |function_|
  // about file selection result (by calling |OnFilePathSelected()|).
  // The |this| object is self destruct after the function is notified.
  // |success| indicates whether user has selected the file.
  // |selected_path| is path that was selected. It is empty if the file wasn't
  // selected.
  void SendResponse(bool success, const base::FilePath& selected_path);

  // Dialog that is shown by selector.
  scoped_refptr<ui::SelectFileDialog> dialog_;

  // Extension function that uses the selector.
  scoped_refptr<FileBrowserHandlerInternalSelectFileFunction> function_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectorImpl);
};

FileSelectorImpl::FileSelectorImpl() = default;

FileSelectorImpl::~FileSelectorImpl() {
  if (dialog_.get())
    dialog_->ListenerDestroyed();
  // Send response if needed.
  if (function_.get())
    SendResponse(false, base::FilePath());
}

void FileSelectorImpl::SelectFile(
    const base::FilePath& suggested_name,
    const std::vector<std::string>& allowed_extensions,
    Browser* browser,
    FileBrowserHandlerInternalSelectFileFunction* function) {
  // We will hold reference to the function until it is notified of selection
  // result.
  function_ = function;

  if (!StartSelectFile(suggested_name, allowed_extensions, browser)) {
    // If the dialog wasn't launched, let's asynchronously report failure to the
    // function.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&FileSelectorImpl::FileSelectionCanceled,
                       base::Unretained(this), static_cast<void*>(nullptr)));
  }
}

bool FileSelectorImpl::StartSelectFile(
    const base::FilePath& suggested_name,
    const std::vector<std::string>& allowed_extensions,
    Browser* browser) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!dialog_.get());
  DCHECK(browser);

  if (!browser->window())
    return false;

  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;

  dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  // Convert |allowed_extensions| to ui::SelectFileDialog::FileTypeInfo.
  ui::SelectFileDialog::FileTypeInfo allowed_file_info =
      ConvertExtensionsToFileTypeInfo(allowed_extensions);
  allowed_file_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH;

  dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE,
      base::string16() /* dialog title*/, suggested_name, &allowed_file_info,
      0 /* file type index */, std::string() /* default file extension */,
      browser->window()->GetNativeWindow(), nullptr /* params */);

  return dialog_->IsRunning(browser->window()->GetNativeWindow());
}

void FileSelectorImpl::FileSelected(
    const base::FilePath& path, int index, void* params) {
  SendResponse(true, path);
  delete this;
}

void FileSelectorImpl::MultiFilesSelected(
    const std::vector<base::FilePath>& files,
    void* params) {
  // Only single file should be selected in save-as dialog.
  NOTREACHED();
}

void FileSelectorImpl::FileSelectionCanceled(
    void* params) {
  SendResponse(false, base::FilePath());
  delete this;
}

void FileSelectorImpl::SendResponse(bool success,
                                    const base::FilePath& selected_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // We don't want to send multiple responses.
  if (function_.get())
    function_->OnFilePathSelected(success, selected_path);
  function_ = nullptr;
}

// FileSelectorFactory implementation.
class FileSelectorFactoryImpl : public FileSelectorFactory {
 public:
  FileSelectorFactoryImpl() = default;
  ~FileSelectorFactoryImpl() override = default;

  // FileSelectorFactory implementation.
  // Creates new FileSelectorImplementation for the function.
  FileSelector* CreateFileSelector() const override {
    return new FileSelectorImpl();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSelectorFactoryImpl);
};

}  // namespace

FileBrowserHandlerInternalSelectFileFunction::
    FileBrowserHandlerInternalSelectFileFunction()
        : file_selector_factory_(new FileSelectorFactoryImpl()),
          user_gesture_check_enabled_(true) {
}

FileBrowserHandlerInternalSelectFileFunction::
    FileBrowserHandlerInternalSelectFileFunction(
        FileSelectorFactory* file_selector_factory,
        bool enable_user_gesture_check)
        : file_selector_factory_(file_selector_factory),
          user_gesture_check_enabled_(enable_user_gesture_check) {
  DCHECK(file_selector_factory);
}

FileBrowserHandlerInternalSelectFileFunction::
    ~FileBrowserHandlerInternalSelectFileFunction() = default;

ExtensionFunction::ResponseAction
FileBrowserHandlerInternalSelectFileFunction::Run() {
  std::unique_ptr<SelectFile::Params> params(
      SelectFile::Params::Create(*args_));

  base::FilePath suggested_name(params->selection_params.suggested_name);
  std::vector<std::string> allowed_extensions;
  if (params->selection_params.allowed_file_extensions.get())
    allowed_extensions = *params->selection_params.allowed_file_extensions;

  if (!user_gesture() && user_gesture_check_enabled_) {
    return RespondNow(Error(kNoUserGestureError));
  }

  FileSelector* file_selector = file_selector_factory_->CreateFileSelector();
  file_selector->SelectFile(
      suggested_name.BaseName(), allowed_extensions,
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser(), this);
  return RespondLater();
}

void FileBrowserHandlerInternalSelectFileFunction::OnFilePathSelected(
    bool success,
    const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!success) {
    RespondWith(EntryDefinition(), false);
    return;
  }

  const ChromeExtensionFunctionDetails chrome_details(this);
  storage::ExternalFileSystemBackend* external_backend =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          chrome_details.GetProfile(), render_frame_host())
          ->external_backend();
  DCHECK(external_backend);

  FileDefinition file_definition;
  file_definition.is_directory = false;

  external_backend->GetVirtualPath(full_path, &file_definition.virtual_path);
  DCHECK(!file_definition.virtual_path.empty());

  // Grant access to this particular file to target extension. This will
  // ensure that the target extension can access only this FS entry and
  // prevent from traversing FS hierarchy upward.
  external_backend->GrantFileAccessToExtension(extension_id(),
                                               file_definition.virtual_path);

  // Grant access to the selected file to target extensions render view process.
  content::ChildProcessSecurityPolicy::GetInstance()->GrantCreateReadWriteFile(
      render_frame_host()->GetProcess()->GetID(), full_path);

  file_manager::util::ConvertFileDefinitionToEntryDefinition(
      chrome_details.GetProfile(), extension_id(), file_definition,
      base::BindOnce(
          &FileBrowserHandlerInternalSelectFileFunction::RespondEntryDefinition,
          this));
}

void FileBrowserHandlerInternalSelectFileFunction::RespondEntryDefinition(
    const EntryDefinition& entry_definition) {
  RespondWith(entry_definition, true);
}

void FileBrowserHandlerInternalSelectFileFunction::RespondWith(
    const EntryDefinition& entry_definition,
    bool success) {
  std::unique_ptr<SelectFile::Results::Result> result(
      new SelectFile::Results::Result());
  result->success = success;

  // If the file was selected, add 'entry' object which will be later used to
  // create a FileEntry instance for the selected file.
  if (success && entry_definition.error == base::File::FILE_OK) {
    result->entry = std::make_unique<FileEntryInfo>();
    // TODO(mtomasz): Make the response fields consistent with other files.
    result->entry->file_system_name = entry_definition.file_system_name;
    result->entry->file_system_root = entry_definition.file_system_root_url;
    result->entry->file_full_path =
        "/" + entry_definition.full_path.AsUTF8Unsafe();
    result->entry->file_is_directory = entry_definition.is_directory;
  }

  Respond(ArgumentList(SelectFile::Results::Create(*result)));
}
