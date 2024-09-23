// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_ENTRY_PICKER_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_ENTRY_PICKER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "extensions/browser/api/file_system/file_system_delegate.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace base {
class FilePath;
}  // namespace base

namespace content {
class WebContents;
}  // namespace content

namespace extensions {

// Shows a dialog to the user to ask for the filename for a file to save or
// open. Deletes itself once the dialog is closed.
class FileEntryPicker : public ui::SelectFileDialog::Listener {
 public:
  // Creates a file picker. After the user picks file(s) or cancels, the
  // relevant callback is called and this object deletes itself.
  // The dialog is modal to the |web_contents|'s window.
  // See SelectFileDialog::SelectFile for the other parameters.
  FileEntryPicker(
      content::WebContents* web_contents,
      const base::FilePath& suggested_name,
      const ui::SelectFileDialog::FileTypeInfo& file_type_info,
      ui::SelectFileDialog::Type picker_type,
      FileSystemDelegate::FilesSelectedCallback files_selected_callback,
      base::OnceClosure file_selection_canceled_callback);

  FileEntryPicker(const FileEntryPicker&) = delete;
  FileEntryPicker& operator=(const FileEntryPicker&) = delete;

 private:
  ~FileEntryPicker() override;  // FileEntryPicker deletes itself.

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void MultiFilesSelected(
      const std::vector<ui::SelectedFileInfo>& files) override;
  void FileSelectionCanceled() override;

  FileSystemDelegate::FilesSelectedCallback files_selected_callback_;
  base::OnceClosure file_selection_canceled_callback_;
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_SYSTEM_FILE_ENTRY_PICKER_H_
