// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/chrome_duplicate_download_infobar_delegate.h"

#include <memory>
#include <optional>

#include "base/android/path_utils.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/ui/android/infobars/duplicate_download_infobar.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "ui/shell_dialogs/selected_file_info.h"

namespace android {

ChromeDuplicateDownloadInfoBarDelegate::
    ~ChromeDuplicateDownloadInfoBarDelegate() {
  if (download_item_)
    download_item_->RemoveObserver(this);
}

// static
void ChromeDuplicateDownloadInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    download::DownloadItem* download_item,
    const base::FilePath& file_path,
    DownloadTargetDeterminerDelegate::ConfirmationCallback callback) {
  infobar_manager->AddInfoBar(DuplicateDownloadInfoBar::CreateInfoBar(
      base::WrapUnique(new ChromeDuplicateDownloadInfoBarDelegate(
          download_item, file_path, std::move(callback)))));
}

void ChromeDuplicateDownloadInfoBarDelegate::OnDownloadDestroyed(
    download::DownloadItem* download_item) {
  DCHECK_EQ(download_item, download_item_);
  download_item_ = nullptr;
}

ChromeDuplicateDownloadInfoBarDelegate::ChromeDuplicateDownloadInfoBarDelegate(
    download::DownloadItem* download_item,
    const base::FilePath& file_path,
    DownloadTargetDeterminerDelegate::ConfirmationCallback
        file_selected_callback)
    : download_item_(download_item),
      file_path_(file_path),
      file_selected_callback_(std::move(file_selected_callback)) {
  download_item_->AddObserver(this);
}

infobars::InfoBarDelegate::InfoBarIdentifier
ChromeDuplicateDownloadInfoBarDelegate::GetIdentifier() const {
  return DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID;
}

bool ChromeDuplicateDownloadInfoBarDelegate::Accept() {
  if (!download_item_) {
    return true;
  }

  base::FilePath download_dir;
  if (!base::android::GetDownloadsDirectory(&download_dir)) {
    return true;
  }

  download::DownloadPathReservationTracker::GetReservedPath(
      download_item_, file_path_, download_dir,
      base::FilePath(), /* fallback_directory */
      true, download::DownloadPathReservationTracker::UNIQUIFY,
      base::BindOnce(&DownloadDialogUtils::CreateNewFileDone,
                     std::move(file_selected_callback_)));
  return true;
}

bool ChromeDuplicateDownloadInfoBarDelegate::Cancel() {
  if (!download_item_)
    return true;

  std::move(file_selected_callback_)
      .Run(DownloadConfirmationResult::CANCELED, ui::SelectedFileInfo());
  return true;
}

std::string ChromeDuplicateDownloadInfoBarDelegate::GetFilePath() const {
  return file_path_.value();
}

void ChromeDuplicateDownloadInfoBarDelegate::InfoBarDismissed() {
  Cancel();
}

std::optional<Profile::OTRProfileID>
ChromeDuplicateDownloadInfoBarDelegate::GetOTRProfileID() const {
  content::BrowserContext* browser_context =
      content::DownloadItemUtils::GetBrowserContext(download_item_);
  // If belongs to an off-the-record profile, then the OTRProfileID should be
  // taken from the browser context to support multiple off-the-record profiles.
  if (browser_context && browser_context->IsOffTheRecord()) {
    return Profile::FromBrowserContext(browser_context)->GetOTRProfileID();
  }
  // If belongs to the regular profile, then OTRProfileID should be null.
  return std::nullopt;
}

}  // namespace android
