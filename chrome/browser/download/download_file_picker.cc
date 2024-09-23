// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_file_picker.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "components/download/public/common/base_file.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "ui/shell_dialogs/selected_file_info.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "ui/aura/window.h"
#endif

using download::DownloadItem;
using content::DownloadManager;
using content::WebContents;

DownloadFilePicker::DownloadFilePicker(download::DownloadItem* item,
                                       const base::FilePath& suggested_path,
                                       ConfirmationCallback callback)
    : suggested_path_(suggested_path),
      file_selected_callback_(std::move(callback)),
      download_item_(item) {
  const DownloadPrefs* prefs = DownloadPrefs::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  DCHECK(prefs);

  DCHECK(item);
  item->AddObserver(this);
  WebContents* web_contents = content::DownloadItemUtils::GetWebContents(item);
  // Extension download may not have associated webcontents.
  if (item->GetDownloadSource() != download::DownloadSource::EXTENSION_API &&
      (!web_contents || !web_contents->GetNativeView())) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFilePicker::FileSelectionCanceled,
                                  base::Unretained(this)));
    return;
  }

  select_file_dialog_ = ui::SelectFileDialog::Create(
      this, std::make_unique<ChromeSelectFilePolicy>(web_contents));
  // |select_file_dialog_| could be null in Linux. See CreateSelectFileDialog()
  // in shell_dialog_linux.cc.
  if (!select_file_dialog_.get()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&DownloadFilePicker::FileSelectionCanceled,
                                  base::Unretained(this)));
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
                   : gfx::NativeWindow();

  // If select_file_dialog_ issued by extension API,
  // (e.g. chrome.downloads.download), the |owning_window| host
  // could be null, then it will cause the select file dialog is not modal
  // dialog in Linux (See SelectFileImpl() in select_file_dialog_linux_gtk.cc).
  // and windows.Here we make owning_window host to browser current active
  // window if it is null. https://crbug.com/1301898
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN)
  if (!owning_window || !owning_window->GetHost()) {
    owning_window = BrowserList::GetInstance()
                        ->GetLastActive()
                        ->window()
                        ->GetNativeWindow();
  }
#endif

  GURL caller = download::BaseFile::GetEffectiveAuthorityURL(
      download_item_->GetURL(), download_item_->GetReferrerUrl());
  // Blob URLs are not set as referrer of downloads of them. If the download url
  // itself has no authority part, their is no authority url. For dlp we want to
  // use the blob's origin in that case.
  auto* render_frame_host =
      content::DownloadItemUtils::GetRenderFrameHost(download_item_);
  if (!caller.is_valid() && render_frame_host &&
      render_frame_host->GetLastCommittedURL().SchemeIsBlob()) {
    caller = render_frame_host->GetLastCommittedOrigin().GetURL();
  }

  select_file_dialog_->SelectFile(
      ui::SelectFileDialog::SELECT_SAVEAS_FILE, std::u16string(),
      suggested_path_, &file_type_info, 0, base::FilePath::StringType(),
      owning_window, &caller);
}

DownloadFilePicker::~DownloadFilePicker() {
  if (select_file_dialog_) {
    select_file_dialog_->ListenerDestroyed();
  }

  if (download_item_) {
    download_item_->RemoveObserver(this);
  }
}

void DownloadFilePicker::FileSelected(const ui::SelectedFileInfo& file,
                                      int index) {
  std::move(file_selected_callback_)
      .Run(DownloadConfirmationResult::CONFIRMED, file);

  delete this;
}

void DownloadFilePicker::FileSelectionCanceled() {
  std::move(file_selected_callback_)
      .Run(DownloadConfirmationResult::CANCELED, ui::SelectedFileInfo());

  delete this;
}

// static
void DownloadFilePicker::ShowFilePicker(DownloadItem* item,
                                        const base::FilePath& suggested_path,
                                        ConfirmationCallback callback) {
  new DownloadFilePicker(item, suggested_path, std::move(callback));
  // DownloadFilePicker deletes itself.
}

void DownloadFilePicker::OnDownloadDestroyed(DownloadItem* download_item) {
  DCHECK_EQ(download_item, download_item_);
  download_item_ = nullptr;
}
