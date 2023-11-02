// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Lacros specific FileBrowserHandlerInternalSelectFileFunctionLacros overrides
// OnFilePathSelected() to do the following:
// * Use storage::IsolatedContext to create extensions::GrantedFileEntry.
// * Grant permissions needed to read/write/create file under the select path.
// * Populate results for API response needed to create JS FileEntry.

#include "chrome/browser/extensions/api/file_manager/file_browser_handler_api_lacros.h"

#include <memory>
#include <utility>

#include "chrome/common/extensions/api/file_browser_handler_internal.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "extensions/browser/granted_file_entry.h"

using content::BrowserThread;
using extensions::api::file_browser_handler_internal::FileEntryInfoForGetFile;
using extensions::app_file_handler_util::CreateFileEntryWithPermissions;

namespace SelectFile =
    extensions::api::file_browser_handler_internal::SelectFile;

FileBrowserHandlerInternalSelectFileFunctionLacros::
    FileBrowserHandlerInternalSelectFileFunctionLacros() = default;

FileBrowserHandlerInternalSelectFileFunctionLacros::
    ~FileBrowserHandlerInternalSelectFileFunctionLacros() = default;

void FileBrowserHandlerInternalSelectFileFunctionLacros::OnFilePathSelected(
    bool success,
    const base::FilePath& full_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!success) {
    RespondWithFailure();
    return;
  }

  int renderer_pid = render_frame_host()->GetProcess()->GetID();
  if (renderer_pid <= 0) {
    RespondWithFailure();
    return;
  }

  extensions::GrantedFileEntry granted_file_entry =
      CreateFileEntryWithPermissions(
          renderer_pid, full_path,
          /* can_write */ true, /* can_create */ true, /* can_delete */ false);

  SelectFile::Results::Result result;
  result.success = true;

  // The value will be consumed by DirectoryEntry.getFile() (JS).
  result.entry_for_get_file.emplace();
  result.entry_for_get_file->file_system_id = granted_file_entry.filesystem_id;
  result.entry_for_get_file->base_name = granted_file_entry.registered_name;
  result.entry_for_get_file->entry_id = granted_file_entry.id;
  result.entry_for_get_file->is_directory = false;

  RespondWithResult(result);
}

template <>
scoped_refptr<ExtensionFunction>
NewExtensionFunction<FileBrowserHandlerInternalSelectFileFunction>() {
  return base::MakeRefCounted<
      FileBrowserHandlerInternalSelectFileFunctionLacros>();
}
