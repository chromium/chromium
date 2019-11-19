// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/chrome_duplicate_download_infobar_delegate.h"

#include <memory>

#include "base/android/path_utils.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/download/android/download_controller.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/duplicate_download_infobar.h"
#include "components/download/public/common/download_path_reservation_tracker.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"

namespace {

void CreateNewFileDone(
    const DownloadTargetDeterminerDelegate::ConfirmationCallback& callback,
    download::PathValidationResult result,
    const base::FilePath& target_path) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result == download::PathValidationResult::SUCCESS)
    callback.Run(DownloadConfirmationResult::CONFIRMED, target_path);
  else
    callback.Run(DownloadConfirmationResult::FAILED, base::FilePath());
}

}  // namespace

namespace android {

ChromeDuplicateDownloadInfoBarDelegate::
    ~ChromeDuplicateDownloadInfoBarDelegate() {
  if (download_item_)
    download_item_->RemoveObserver(this);
}

// static
void ChromeDuplicateDownloadInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    download::DownloadItem* download_item,
    const base::FilePath& file_path,
    const DownloadTargetDeterminerDelegate::ConfirmationCallback& callback) {
  infobar_service->AddInfoBar(DuplicateDownloadInfoBar::CreateInfoBar(
      base::WrapUnique(new ChromeDuplicateDownloadInfoBarDelegate(
          download_item, file_path, callback))));
}

void ChromeDuplicateDownloadInfoBarDelegate::OnDownloadDestroyed(
    download::DownloadItem* download_item) {
  DCHECK_EQ(download_item, download_item_);
  download_item_ = nullptr;
}

ChromeDuplicateDownloadInfoBarDelegate::ChromeDuplicateDownloadInfoBarDelegate(
    download::DownloadItem* download_item,
    const base::FilePath& file_path,
    const DownloadTargetDeterminerDelegate::ConfirmationCallback&
        file_selected_callback)
    : download_item_(download_item),
      file_path_(file_path),
      is_off_the_record_(
          content::DownloadItemUtils::GetBrowserContext(download_item)
              ->IsOffTheRecord()),
      file_selected_callback_(file_selected_callback) {
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
      base::Bind(&CreateNewFileDone, file_selected_callback_));
  return true;
}

bool ChromeDuplicateDownloadInfoBarDelegate::Cancel() {
  if (!download_item_)
    return true;

  file_selected_callback_.Run(DownloadConfirmationResult::CANCELED,
                              base::FilePath());
  return true;
}

std::string ChromeDuplicateDownloadInfoBarDelegate::GetFilePath() const {
  return file_path_.value();
}

void ChromeDuplicateDownloadInfoBarDelegate::InfoBarDismissed() {
  Cancel();
}

bool ChromeDuplicateDownloadInfoBarDelegate::IsOffTheRecord() const {
  return is_off_the_record_;
}

}  // namespace android
