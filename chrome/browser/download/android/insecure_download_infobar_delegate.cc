// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/insecure_download_infobar_delegate.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/download/public/common/download_item.h"
#include "components/infobars/android/confirm_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

using InsecureDownloadStatus = download::DownloadItem::InsecureDownloadStatus;

// static
void InsecureDownloadInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager,
    const base::FilePath& basename,
    download::DownloadItem::InsecureDownloadStatus insecure_download_status,
    ResultCallback callback) {
  infobar_manager->AddInfoBar(std::make_unique<infobars::ConfirmInfoBar>(
      base::WrapUnique(new InsecureDownloadInfoBarDelegate(
          basename, insecure_download_status, std::move(callback)))));
}

InsecureDownloadInfoBarDelegate::InsecureDownloadInfoBarDelegate(
    const base::FilePath& basename,
    download::DownloadItem::InsecureDownloadStatus insecure_download_status,
    ResultCallback callback)
    : insecure_download_status_(insecure_download_status),
      callback_(std::move(callback)) {
  message_text_ =
      l10n_util::GetStringFUTF16(IDS_PROMPT_CONFIRM_INSECURE_DOWNLOAD,
                                 base::UTF8ToUTF16(basename.value()));
}

InsecureDownloadInfoBarDelegate::~InsecureDownloadInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
InsecureDownloadInfoBarDelegate::GetIdentifier() const {
  return INSECURE_DOWNLOAD_INFOBAR_DELEGATE_ANDROID;
}

int InsecureDownloadInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_WARNING;
}

bool InsecureDownloadInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

void InsecureDownloadInfoBarDelegate::InfoBarDismissed() {
  PostReply(false);
}

std::u16string InsecureDownloadInfoBarDelegate::GetMessageText() const {
  return message_text_;
}

std::u16string InsecureDownloadInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  if (insecure_download_status_ == InsecureDownloadStatus::WARN) {
    return l10n_util::GetStringUTF16(
        button == BUTTON_OK ? IDS_CONFIRM_DOWNLOAD : IDS_DISCARD_DOWNLOAD);
  }

  DCHECK_EQ(insecure_download_status_, InsecureDownloadStatus::BLOCK);
  // Default button is Discard when blocking.
  return l10n_util::GetStringUTF16(button == BUTTON_OK ? IDS_DISCARD_DOWNLOAD
                                                       : IDS_CONFIRM_DOWNLOAD);
}

bool InsecureDownloadInfoBarDelegate::Accept() {
  if (insecure_download_status_ == InsecureDownloadStatus::WARN) {
    PostReply(true);
    return true;
  }

  DCHECK_EQ(insecure_download_status_, InsecureDownloadStatus::BLOCK);
  // Default button is Discard when blocking.
  PostReply(false);
  return true;
}

bool InsecureDownloadInfoBarDelegate::Cancel() {
  if (insecure_download_status_ == InsecureDownloadStatus::WARN) {
    PostReply(false);
    return true;
  }

  CHECK_EQ(insecure_download_status_, InsecureDownloadStatus::BLOCK);
  // Cancel button is Keep when blocking.
  PostReply(true);
  return true;
}

void InsecureDownloadInfoBarDelegate::PostReply(bool should_download) {
  DCHECK(callback_);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_), should_download));
}
