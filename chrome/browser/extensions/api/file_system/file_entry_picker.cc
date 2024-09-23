// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_entry_picker.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/file_system/file_system_delegate.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace extensions {

FileEntryPicker::FileEntryPicker(
    content::WebContents* web_contents,
    const base::FilePath& suggested_name,
    const ui::SelectFileDialog::FileTypeInfo& file_type_info,
    ui::SelectFileDialog::Type picker_type,
    FileSystemDelegate::FilesSelectedCallback files_selected_callback,
    base::OnceClosure file_selection_canceled_callback)
    : files_selected_callback_(std::move(files_selected_callback)),
      file_selection_canceled_callback_(
          std::move(file_selection_canceled_callback)) {
  CHECK(web_contents);
  gfx::NativeWindow owning_window =
      platform_util::GetTopLevel(web_contents->GetNativeView());
  const GURL* caller =
      &web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));

  select_file_dialog_->SelectFile(
      picker_type, std::u16string(), suggested_name, &file_type_info, 0,
      base::FilePath::StringType(), owning_window, caller);
}

FileEntryPicker::~FileEntryPicker() {
  select_file_dialog_->ListenerDestroyed();
}

void FileEntryPicker::FileSelected(const ui::SelectedFileInfo& file,
                                   int index) {
  MultiFilesSelected({file});
}

void FileEntryPicker::MultiFilesSelected(
    const std::vector<ui::SelectedFileInfo>& files) {
  std::vector<base::FilePath> paths;
  for (const auto& file : files) {
    // Normally, `file.local_path` is used because it is a native path to the
    // local read-only cached file in the case of remote file system like
    // ChromeOS's Google Drive integration. Here, however, `file.file_path` is
    // necessary because we need to create a FileEntry denoting the remote file,
    // not its cache. On other platforms than Chrome OS, they are the same.
    //
    // TODO(kinaba): remove this, once after the file picker implements proper
    // switch of the path treatment depending on the `allowed_paths`.
    paths.push_back(file.file_path);
  }
  std::move(files_selected_callback_).Run(paths);
  delete this;
}

void FileEntryPicker::FileSelectionCanceled() {
  std::move(file_selection_canceled_callback_).Run();
  delete this;
}

}  // namespace extensions
