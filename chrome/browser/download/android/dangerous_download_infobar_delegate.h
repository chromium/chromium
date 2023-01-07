// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_DANGEROUS_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_DANGEROUS_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "base/memory/raw_ptr.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

namespace infobars {
class ContentInfoBarManager;
}

// An infobar that asks if user wants to download a dangerous file.
// Note that this infobar does not expire if the user subsequently navigates,
// since such navigations won't automatically cancel the underlying download.
class DangerousDownloadInfoBarDelegate
    : public ConfirmInfoBarDelegate,
      public download::DownloadItem::Observer {
 public:
  static void Create(infobars::ContentInfoBarManager* infobar_manager,
                     download::DownloadItem* download_item);

  DangerousDownloadInfoBarDelegate(const DangerousDownloadInfoBarDelegate&) =
      delete;
  DangerousDownloadInfoBarDelegate& operator=(
      const DangerousDownloadInfoBarDelegate&) = delete;

  ~DangerousDownloadInfoBarDelegate() override;

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download_item) override;

 private:
  explicit DangerousDownloadInfoBarDelegate(
      download::DownloadItem* download_item);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  void InfoBarDismissed() override;
  std::u16string GetMessageText() const override;
  bool Accept() override;
  bool Cancel() override;

  // The download item that is requesting the infobar. Could get deleted while
  // the infobar is showing.
  raw_ptr<download::DownloadItem> download_item_;
  std::u16string message_text_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_DANGEROUS_DOWNLOAD_INFOBAR_DELEGATE_H_
