// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/downloads/offline_page_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/download/android/download_dialog_utils.h"
#include "chrome/browser/ui/android/infobars/duplicate_download_infobar.h"
#include "components/infobars/content/content_infobar_manager.h"

namespace offline_pages {

// static
void OfflinePageInfoBarDelegate::Create(base::OnceClosure confirm_continuation,
                                        const GURL& page_to_download,
                                        bool exists_duplicate_request,
                                        content::WebContents* web_contents) {
  infobars::ContentInfoBarManager::FromWebContents(web_contents)
      ->AddInfoBar(DuplicateDownloadInfoBar::CreateInfoBar(
          base::WrapUnique(new OfflinePageInfoBarDelegate(
              std::move(confirm_continuation),
              DownloadDialogUtils::GetDisplayURLForPageURL(page_to_download),
              page_to_download, exists_duplicate_request))));
}

OfflinePageInfoBarDelegate::~OfflinePageInfoBarDelegate() {}

OfflinePageInfoBarDelegate::OfflinePageInfoBarDelegate(
    base::OnceClosure confirm_continuation,
    const std::string& page_name,
    const GURL& page_to_download,
    bool duplicate_request_exists)
    : confirm_continuation_(std::move(confirm_continuation)),
      page_name_(page_name),
      page_to_download_(page_to_download),
      duplicate_request_exists_(duplicate_request_exists) {}

infobars::InfoBarDelegate::InfoBarIdentifier
OfflinePageInfoBarDelegate::GetIdentifier() const {
  return OFFLINE_PAGE_INFOBAR_DELEGATE_ANDROID;
}

bool OfflinePageInfoBarDelegate::EqualsDelegate(
    InfoBarDelegate* delegate) const {
  OfflinePageInfoBarDelegate* confirm_delegate =
      delegate->AsOfflinePageInfoBarDelegate();
  return confirm_delegate && GetFilePath() == confirm_delegate->GetFilePath();
}

bool OfflinePageInfoBarDelegate::Cancel() {
  return true;
}

bool OfflinePageInfoBarDelegate::Accept() {
  std::move(confirm_continuation_).Run();
  return true;
}

std::string OfflinePageInfoBarDelegate::GetFilePath() const {
  return page_name_;
}

bool OfflinePageInfoBarDelegate::IsOfflinePage() const {
  return true;
}

std::string OfflinePageInfoBarDelegate::GetPageURL() const {
  return page_to_download_.spec();
}

bool OfflinePageInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return InfoBarDelegate::ShouldExpire(details);
}

bool OfflinePageInfoBarDelegate::DuplicateRequestExists() const {
  return duplicate_request_exists_;
}

OfflinePageInfoBarDelegate*
OfflinePageInfoBarDelegate::AsOfflinePageInfoBarDelegate() {
  return this;
}

void OfflinePageInfoBarDelegate::InfoBarDismissed() {}

}  // namespace offline_pages
