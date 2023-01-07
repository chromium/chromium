// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_SELECTOR_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_SELECTOR_IMPL_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/extensions/api/file_manager/file_selector.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class Browser;

namespace file_manager {

// Self-deleting main implementation of FileSelector that uses
// ui::SelectFileDialog.
class FileSelectorImpl final : public FileSelector,
                               public ui::SelectFileDialog::Listener {
 public:
  FileSelectorImpl();
  FileSelectorImpl(const FileSelectorImpl&) = delete;
  FileSelectorImpl& operator=(const FileSelectorImpl&) = delete;
  ~FileSelectorImpl() override;

 protected:
  // file_manager::FileSelector overrides.
  void SelectFile(const base::FilePath& suggested_name,
                  const std::vector<std::string>& allowed_extensions,
                  Browser* browser,
                  OnSelectedCallback callback) override;

  // ui::SelectFileDialog::Listener overrides.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void MultiFilesSelected(const std::vector<base::FilePath>& files,
                          void* params) override;
  void FileSelectionCanceled(void* params) override;

 private:
  // Performs simple checks and attempts to show the "Save as" dialog to the
  // user. Returns whether dialog successfully got shown. If not, then the
  // attempt is considered aborted. Otherwise expects asynchronous response as
  // calls to one of the ui::SelectFileDialog::Listener overrides, depending on
  // user action.
  bool StartSelectFile(const base::FilePath& suggested_name,
                       const std::vector<std::string>& allowed_extensions,
                       Browser* browser);

  // Reacts to the user action reported by the dialog and passes the file
  // selection results to |callback_|. The |this| object is self destruct after
  // the function is notified. |success| indicates whether user has selected the
  // file. |selected_path| is path that was selected. It is empty if the file
  // wasn't selected.
  void SendResponse(bool success, const base::FilePath& selected_path);

  // Dialog shown by selector.
  scoped_refptr<ui::SelectFileDialog> dialog_;

  // Callback to receive results.
  OnSelectedCallback callback_;
};

// FileSelectorFactory implementation.
class FileSelectorFactoryImpl final : public FileSelectorFactory {
 public:
  FileSelectorFactoryImpl();
  FileSelectorFactoryImpl(const FileSelectorFactoryImpl&) = delete;
  FileSelectorFactoryImpl& operator=(const FileSelectorFactoryImpl&) = delete;
  ~FileSelectorFactoryImpl() override;

  // FileSelectorFactory overrides.
  FileSelector* CreateFileSelector() const override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_EXTENSIONS_API_FILE_MANAGER_FILE_SELECTOR_IMPL_H_
