// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker.h"

#include "base/bind.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"

using download::DownloadItem;
using content::DownloadManager;
using content::WebContents;

DownloadFilePicker::DownloadFilePicker(DownloadItem* item,
                                       const base::FilePath& suggested_path,
                                       ConfirmationCallback callback)
    : suggested_path_(suggested_path),
      file_selected_callback_(std::move(callback)) {
  const DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  DCHECK(prefs);

  WebContents* web_contents = content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents || !web_contents->GetNativeView()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFilePicker::FileSelectionCanceled,
                                  base::Unretained(this), nullptr));
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  // |select_file_dialog_| could be null in Linux. See CreateSelectFileDialog()
  // in shell_dialog_linux.cc.
  if (!select_file_dialog_.get()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFilePicker::FileSelectionCanceled,
                                  base::Unretained(this), nullptr));
    return;
  }

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  // Platform file pickers, notably on Mac and Windows, tend to break
  // with double extensions like .tar.gz, so only pass in normal ones.
  base::FilePath::StringType extension = suggested_path_.FinalExtension();
  if (!extension.empty()) {
    extension.erase(extension.begin());  // drop the .
    file_type_info.extensions.resize(1);
    file_type_info.extensions[0].push_back(extension);
  }
  file_type_info.include_all_files = true;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::NATIVE_PATH;
  gfx::NativeWindow owning_window =
      web_contents ? platform_util::GetTopLevel(web_contents->GetNativeView())
                   : gfx::kNullNativeWindow;

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
      suggested_path_, &file_type_info, 0, base::FilePath::StringType(),
      owning_window, NULL);
}

DownloadFilePicker::~DownloadFilePicker() {
  if (select_file_dialog_)
    select_file_dialog_->ListenerDestroyed();
}

void DownloadFilePicker::OnFileSelected(const base::FilePath& path) {
  std::move(file_selected_callback_)
      .Run(path.empty() ? DownloadConfirmationResult::CANCELED
                        : DownloadConfirmationResult::CONFIRMED,
           path);
  delete this;
}

void DownloadFilePicker::FileSelected(const base::FilePath& path,
                                      int index,
                                      void* params) {
  OnFileSelected(path);
  // Deletes |this|
}

void DownloadFilePicker::FileSelectionCanceled(void* params) {
  OnFileSelected(base::FilePath());
  // Deletes |this|
}

// static
void DownloadFilePicker::ShowFilePicker(DownloadItem* item,
                                        const base::FilePath& suggested_path,
                                        ConfirmationCallback callback) {
  new DownloadFilePicker(item, suggested_path, std::move(callback));
  // DownloadFilePicker deletes itself.
}
