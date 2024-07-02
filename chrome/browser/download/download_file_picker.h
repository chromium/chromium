// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/download_confirmation_result.h"
#include "components/download/public/common/download_item.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace base {
class FilePath;
}

// Handles showing a dialog to the user to ask for the filename for a download.
class DownloadFilePicker : public ui::SelectFileDialog::Listener,
                           public download::DownloadItem::Observer {
 public:
  // Callback used to pass the user selection back to the owner of this object.
  // `selected_file_info` is the file info chosen by the user, or a value with
  // an empty path if the user cancels the file selection.
  using ConfirmationCallback =
      base::OnceCallback<void(DownloadConfirmationResult,
                              const ui::SelectedFileInfo& selected_file_info)>;

  DownloadFilePicker(const DownloadFilePicker&) = delete;
  DownloadFilePicker& operator=(const DownloadFilePicker&) = delete;

  // Display a file picker dialog for `item`. The `suggested_path` will be used
  // as the initial path displayed to the user. `callback` will always be
  // invoked even if `item` is destroyed prior to the file picker completing.
  static void ShowFilePicker(download::DownloadItem* item,
                             const base::FilePath& suggested_path,
                             ConfirmationCallback callback);

 private:
  DownloadFilePicker(download::DownloadItem* item,
                     const base::FilePath& suggested_path,
                     ConfirmationCallback callback);
  ~DownloadFilePicker() override;

  // SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

  // DownloadItem::Observer
  void OnDownloadDestroyed(download::DownloadItem* download) override;

  // Initially suggested path.
  base::FilePath suggested_path_;

  // Callback invoked when a file selection is complete.
  ConfirmationCallback file_selected_callback_;

  // For managing select file dialogs.
  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  // The item to be downloaded.
  raw_ptr<download::DownloadItem> download_item_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_FILE_PICKER_H_
