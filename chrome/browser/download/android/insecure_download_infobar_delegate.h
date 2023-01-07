// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_INSECURE_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_INSECURE_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

// An infobar that asks if user wants to download an insecurely delivered file.
// Note that this infobar does not expire if the user subsequently navigates,
// since such navigations won't automatically cancel the underlying download.
class InsecureDownloadInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using ResultCallback = base::OnceCallback<void(bool should_download)>;

  static void Create(
      infobars::ContentInfoBarManager* infobar_manager,
      const base::FilePath& basename,
      download::DownloadItem::InsecureDownloadStatus insecure_download_status,
      ResultCallback callback);

  InsecureDownloadInfoBarDelegate(const InsecureDownloadInfoBarDelegate&) =
      delete;
  InsecureDownloadInfoBarDelegate& operator=(
      const InsecureDownloadInfoBarDelegate&) = delete;

  ~InsecureDownloadInfoBarDelegate() override;

 private:
  explicit InsecureDownloadInfoBarDelegate(
      const base::FilePath& basename,
      download::DownloadItem::InsecureDownloadStatus insecure_download_status,
      ResultCallback callback);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;
  bool Cancel() override;

  // Calls callback_ with the appropriate result.
  void PostReply(bool should_download);

  std::u16string message_text_;
  download::DownloadItem::InsecureDownloadStatus insecure_download_status_;
  ResultCallback callback_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_INSECURE_DOWNLOAD_INFOBAR_DELEGATE_H_
