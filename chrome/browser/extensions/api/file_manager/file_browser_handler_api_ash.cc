// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Ash specific FileBrowserHandlerInternalSelectFileFunctionAsh overrides
// OnFilePathSelected() to do the following:
// * Open external file system needed to create FileEntry object for the
//   selected path (opening file system will create file system name and root
//   url for the caller's external file system).
// * Grant permissions needed to read/write/create file under the select path.
//   To grant permissions to the caller, caller's extension ID has to be allowed
//   to access the file's virtual path (e.g. /Downloads/foo) in
//   ExternalFileSystemBackend. Additionally, the caller's render process ID has
//   to be granted read, write and create permissions for the selected file's
//   full filesystem path (e.g. /home/chronos/user/Downloads/foo) in
//   ChildProcessSecurityPolicy.
// * Populate results for API response needed to create JS FileEntry.

#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api_ash.h"

#include <utility>

#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/extensions/api/file_manager/file_selector.h"
#include "chrome/browser/extensions/api/file_manager/file_selector_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/file_browser_handler_internal.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "storage/browser/file_system/file_system_backend.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/common/file_system/file_system_util.h"

using content::BrowserThread;
using extensions::api::file_browser_handler_internal::FileEntryInfo;
using file_manager::FileSelectorFactory;
using file_manager::util::EntryDefinition;
using file_manager::util::FileDefinition;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

FileBrowserHandlerInternalSelectFileFunctionAsh::
    FileBrowserHandlerInternalSelectFileFunctionAsh() = default;

FileBrowserHandlerInternalSelectFileFunctionAsh::
    FileBrowserHandlerInternalSelectFileFunctionAsh(
        FileSelectorFactory* file_selector_factory,
        bool enable_user_gesture_check)
    : FileBrowserHandlerInternalSelectFileFunction(file_selector_factory,
                                                   enable_user_gesture_check) {}

FileBrowserHandlerInternalSelectFileFunctionAsh::
    ~FileBrowserHandlerInternalSelectFileFunctionAsh() = default;

void FileBrowserHandlerInternalSelectFileFunctionAsh::OnFilePathSelected(
    bool success,
    const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!success) {
    RespondWithFailure();
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
  if (entry_definition.error != base::File::FILE_OK) {
    RespondWithFailure();
    return;
  }

  SelectFile::Results::Result result;
  result.success = true;

  // If the file was selected, add "entry" object which will be later used to
  // create a FileEntry instance for the selected file.
  // The value will be consumed by GetExternalFileEntry() (JS).
  result.entry.emplace();
  // TODO(mtomasz): Make the response fields consistent with other files.
  result.entry->file_system_name = entry_definition.file_system_name;
  result.entry->file_system_root = entry_definition.file_system_root_url;
  result.entry->file_full_path =
      "/" + entry_definition.full_path.AsUTF8Unsafe();
  result.entry->file_is_directory = entry_definition.is_directory;

  RespondWithResult(result);
}

template <>
scoped_refptr<ExtensionFunction>
NewExtensionFunction<FileBrowserHandlerInternalSelectFileFunction>() {
  return base::MakeRefCounted<
      FileBrowserHandlerInternalSelectFileFunctionAsh>();
}
