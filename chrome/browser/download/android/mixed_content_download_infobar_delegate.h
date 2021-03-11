// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/core/confirm_infobar_delegate.h"

class InfoBarService;

// An infobar that asks if user wants to download an insecurely delivered file
// initiated from a secure context.  Note that this infobar does not expire if
// the user subsequently navigates, since such navigations won't automatically
// cancel the underlying download.
class MixedContentDownloadInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  using ResultCallback = base::OnceCallback<void(bool should_download)>;

  static void Create(
      InfoBarService* infobar_service,
      const base::FilePath& basename,
      download::DownloadItem::MixedContentStatus mixed_content_status,
      ResultCallback callback);

  ~MixedContentDownloadInfoBarDelegate() override;

 private:
  explicit MixedContentDownloadInfoBarDelegate(
      const base::FilePath& basename,
      download::DownloadItem::MixedContentStatus mixed_content_status,
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
  download::DownloadItem::MixedContentStatus mixed_content_status_;
  ResultCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MixedContentDownloadInfoBarDelegate);
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_MIXED_CONTENT_DOWNLOAD_INFOBAR_DELEGATE_H_
