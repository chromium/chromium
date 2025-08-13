// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_select_file_dialog.h"

#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/select_file_dialog.h"
#include "ui/shell_dialogs/selected_file_info.h"

// static
void DevToolsSelectFileDialog::SelectFile(content::WebContents* web_contents,
                                          ui::SelectFileDialog::Type type,
                                          SelectedCallback selected_callback,
                                          CanceledCallback canceled_callback,
                                          const base::FilePath& default_path) {
  // `dialog` is self-deleting.
  auto* dialog = new DevToolsSelectFileDialog(
      web_contents, std::move(selected_callback), std::move(canceled_callback));
  dialog->Show(web_contents, type, default_path);
}

void DevToolsSelectFileDialog::FileSelected(const ui::SelectedFileInfo& file,
                                            int index) {
  std::move(selected_callback_).Run(file);
  delete this;
}

void DevToolsSelectFileDialog::FileSelectionCanceled() {
  if (canceled_callback_) {
    std::move(canceled_callback_).Run();
  }
  delete this;
}

DevToolsSelectFileDialog::DevToolsSelectFileDialog(
    content::WebContents* web_contents,
    SelectedCallback selected_callback,
    CanceledCallback canceled_callback)
    : select_file_dialog_(ui::SelectFileDialog::Create(
          this,
          std::make_unique<ChromeSelectFilePolicy>(web_contents))),
      selected_callback_(std::move(selected_callback)),
      canceled_callback_(std::move(canceled_callback)) {}

DevToolsSelectFileDialog::~DevToolsSelectFileDialog() {
  select_file_dialog_->ListenerDestroyed();
}

void DevToolsSelectFileDialog::Show(content::WebContents* web_contents,
                                    ui::SelectFileDialog::Type type,
                                    const base::FilePath& default_path) {
  base::FilePath::StringType ext;
  ui::SelectFileDialog::FileTypeInfo file_type_info;
  if (type == ui::SelectFileDialog::SELECT_SAVEAS_FILE &&
      default_path.Extension().length() > 0) {
    ext = default_path.Extension().substr(1);
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(ext);
  }
  select_file_dialog_->SelectFile(
      type, std::u16string(), default_path, &file_type_info, 0, ext,
      platform_util::GetTopLevel(web_contents->GetNativeView()));
}
