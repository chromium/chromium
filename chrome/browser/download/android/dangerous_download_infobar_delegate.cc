// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/dangerous_download_infobar_delegate.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"

// static
void DangerousDownloadInfoBarDelegate::Create(
    InfoBarService* infobar_service,
    download::DownloadItem* download_item) {
  infobar_service->AddInfoBar(infobar_service->CreateConfirmInfoBar(
    base::WrapUnique(new DangerousDownloadInfoBarDelegate(download_item))));
}

DangerousDownloadInfoBarDelegate::DangerousDownloadInfoBarDelegate(
    download::DownloadItem* download_item)
    : download_item_(download_item) {
  download_item_->AddObserver(this);
  message_text_ = l10n_util::GetStringFUTF16(
      IDS_PROMPT_DANGEROUS_DOWNLOAD,
      base::UTF8ToUTF16(download_item_->GetFileNameToReportUser().value()));
}

DangerousDownloadInfoBarDelegate::~DangerousDownloadInfoBarDelegate() {
  if (download_item_)
    download_item_->RemoveObserver(this);
}

void DangerousDownloadInfoBarDelegate::OnDownloadDestroyed(
    download::DownloadItem* download_item) {
  DCHECK_EQ(download_item, download_item_);
  download_item_ = nullptr;
}

infobars::InfoBarDelegate::InfoBarIdentifier
DangerousDownloadInfoBarDelegate::GetIdentifier() const {
  return DANGEROUS_DOWNLOAD_INFOBAR_DELEGATE_ANDROID;
}

int DangerousDownloadInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_WARNING;
}

bool DangerousDownloadInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void DangerousDownloadInfoBarDelegate::InfoBarDismissed() {
  if (download_item_)
    download_item_->Remove();
}

base::string16 DangerousDownloadInfoBarDelegate::GetMessageText() const {
  return message_text_;
}

bool DangerousDownloadInfoBarDelegate::Accept() {
  if (download_item_)
    download_item_->ValidateDangerousDownload();
  return true;
}

bool DangerousDownloadInfoBarDelegate::Cancel() {
  if (download_item_)
    download_item_->Remove();
  return true;
}
