// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_CHROME_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_CHROME_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/download/android/duplicate_download_infobar_delegate.h"
#include "chrome/browser/download/download_target_determiner_delegate.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/core/infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

namespace android {

// An infobar delegate that starts from the given file path.
class ChromeDuplicateDownloadInfoBarDelegate
    : public DuplicateDownloadInfoBarDelegate,
      public download::DownloadItem::Observer {
 public:
  ChromeDuplicateDownloadInfoBarDelegate(
      const ChromeDuplicateDownloadInfoBarDelegate&) = delete;
  ChromeDuplicateDownloadInfoBarDelegate& operator=(
      const ChromeDuplicateDownloadInfoBarDelegate&) = delete;

  ~ChromeDuplicateDownloadInfoBarDelegate() override;

  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     download::DownloadItem* download_item,
                     const base::FilePath& file_path,
                     DownloadTargetDeterminerDelegate::ConfirmationCallback
                         file_selected_callback);

  // download::DownloadItem::Observer
  void OnDownloadDestroyed(download::DownloadItem* download_item) override;

 private:
  ChromeDuplicateDownloadInfoBarDelegate(
      download::DownloadItem* download_item,
      const base::FilePath& file_path,
      DownloadTargetDeterminerDelegate::ConfirmationCallback callback);

  // DownloadOverwriteInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool Accept() override;
  bool Cancel() override;
  std::string GetFilePath() const override;
  void InfoBarDismissed() override;
  std::optional<Profile::OTRProfileID> GetOTRProfileID() const override;

  // The download item that is requesting the infobar. Could get deleted while
  // the infobar is showing.
  raw_ptr<download::DownloadItem> download_item_;

  // The target file path to be downloaded. This is used to show users the
  // file name that will be used.
  base::FilePath file_path_;

  // A callback to download target determiner to notify that file selection
  // is made (or cancelled).
  DownloadTargetDeterminerDelegate::ConfirmationCallback
      file_selected_callback_;
};

}  // namespace android

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_CHROME_DUPLICATE_DOWNLOAD_INFOBAR_DELEGATE_H_
