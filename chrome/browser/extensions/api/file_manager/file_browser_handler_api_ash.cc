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
//    FileSelectorImpl notifies FileBrowserHandlerInternalSelectFileFunctionAsh
//    of the selection result by calling
//    FileHandlerSelectFile::OnFilePathSelected.
//  - If the selection was canceled,
//    FileBrowserHandlerInternalSelectFileFunctionAsh returns reporting failure.
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

#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api_ash.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/api/file_manager/file_selector_impl.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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

using content::BrowserThread;
using extensions::api::file_browser_handler_internal::FileEntryInfo;
using file_manager::FileSelector;
using file_manager::FileSelectorFactory;
using file_manager::FileSelectorFactoryImpl;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

namespace {

const char kNoUserGestureError[] =
    "This method can only be called in response to user gesture, such as a "
    "mouse click or key press.";

}  // namespace

FileBrowserHandlerInternalSelectFileFunctionAsh::
    FileBrowserHandlerInternalSelectFileFunctionAsh()
    : file_selector_factory_(new FileSelectorFactoryImpl()),
      user_gesture_check_enabled_(true) {}

FileBrowserHandlerInternalSelectFileFunctionAsh::
    FileBrowserHandlerInternalSelectFileFunctionAsh(
        FileSelectorFactory* file_selector_factory,
        bool enable_user_gesture_check)
    : file_selector_factory_(file_selector_factory),
      user_gesture_check_enabled_(enable_user_gesture_check) {
  DCHECK(file_selector_factory);
}

FileBrowserHandlerInternalSelectFileFunctionAsh::
    ~FileBrowserHandlerInternalSelectFileFunctionAsh() = default;

ExtensionFunction::ResponseAction
FileBrowserHandlerInternalSelectFileFunctionAsh::Run() {
  std::unique_ptr<SelectFile::Params> params(
      SelectFile::Params::Create(args()));

  base::FilePath suggested_name(params->selection_params.suggested_name);
  std::vector<std::string> allowed_extensions;
  if (params->selection_params.allowed_file_extensions.get())
    allowed_extensions = *params->selection_params.allowed_file_extensions;

  if (!user_gesture() && user_gesture_check_enabled_) {
    return RespondNow(Error(kNoUserGestureError));
  }

  FileSelector* file_selector = file_selector_factory_->CreateFileSelector();
  auto callback = base::BindOnce(
      &FileBrowserHandlerInternalSelectFileFunctionAsh::OnFilePathSelected,
      this);
  file_selector->SelectFile(
      suggested_name.BaseName(), allowed_extensions,
      ChromeExtensionFunctionDetails(this).GetCurrentBrowser(),
      std::move(callback));
  return RespondLater();
}

void FileBrowserHandlerInternalSelectFileFunctionAsh::OnFilePathSelected(
    bool success,
    const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!success) {
    RespondWith(EntryDefinition(), false);
    return;
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  storage::ExternalFileSystemBackend* external_backend =
      file_manager::util::GetFileSystemContextForRenderFrameHost(
          profile, render_frame_host())
          ->external_backend();
  DCHECK(external_backend);

  FileDefinition file_definition;
  file_definition.is_directory = false;

  external_backend->GetVirtualPath(full_path, &file_definition.virtual_path);
  DCHECK(!file_definition.virtual_path.empty());

  // Grant access to this particular file to the caller with the given origin.
  // This will ensure that the caller can access only this FS entry and
  // prevent from traversing FS hierarchy upward.
  const url::Origin caller_origin = url::Origin::Create(source_url());
  external_backend->GrantFileAccessToOrigin(caller_origin,
                                            file_definition.virtual_path);

  // Grant access to the selected file to target extensions render view process.
  content::ChildProcessSecurityPolicy::GetInstance()->GrantCreateReadWriteFile(
      render_frame_host()->GetProcess()->GetID(), full_path);

  file_manager::util::ConvertFileDefinitionToEntryDefinition(
      file_manager::util::GetFileSystemContextForSourceURL(profile,
                                                           source_url()),
      caller_origin, file_definition,
      base::BindOnce(&FileBrowserHandlerInternalSelectFileFunctionAsh::
                         RespondEntryDefinition,
                     this));
}

void FileBrowserHandlerInternalSelectFileFunctionAsh::RespondEntryDefinition(
    const EntryDefinition& entry_definition) {
  RespondWith(entry_definition, true);
}

void FileBrowserHandlerInternalSelectFileFunctionAsh::RespondWith(
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

template <>
scoped_refptr<ExtensionFunction>
NewExtensionFunction<FileBrowserHandlerInternalSelectFileFunction>() {
  return base::MakeRefCounted<
      FileBrowserHandlerInternalSelectFileFunctionAsh>();
}
