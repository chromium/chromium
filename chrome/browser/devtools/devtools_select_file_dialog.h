// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SELECT_FILE_DIALOG_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SELECT_FILE_DIALOG_H_

#include "base/functional/callback.h"
#include "ui/shell_dialogs/select_file_dialog.h"

namespace content {
class WebContents;
}

class DevToolsSelectFileDialog : public ui::SelectFileDialog::Listener {
 public:
  using SelectedCallback =
      base::OnceCallback<void(const ui::SelectedFileInfo&)>;
  using CanceledCallback = base::OnceClosure;

  DevToolsSelectFileDialog(const DevToolsSelectFileDialog&) = delete;
  DevToolsSelectFileDialog& operator=(const DevToolsSelectFileDialog&) = delete;

  static void SelectFile(content::WebContents* web_contents,
                         ui::SelectFileDialog::Type type,
                         SelectedCallback selected_callback,
                         CanceledCallback canceled_callback,
                         const base::FilePath& default_path);

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const ui::SelectedFileInfo& file, int index) override;
  void FileSelectionCanceled() override;

 private:
  DevToolsSelectFileDialog(content::WebContents* web_contents,
                           SelectedCallback selected_callback,
                           CanceledCallback canceled_callback);
  ~DevToolsSelectFileDialog() override;

  void Show(content::WebContents* web_contents,
            ui::SelectFileDialog::Type type,
            const base::FilePath& default_path);

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;
  SelectedCallback selected_callback_;
  CanceledCallback canceled_callback_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_SELECT_FILE_DIALOG_H_
