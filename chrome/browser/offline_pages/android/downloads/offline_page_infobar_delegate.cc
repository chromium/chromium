// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/android/downloads/offline_page_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/android/infobars/duplicate_download_infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/text_elider.h"

namespace offline_pages {

// static
void OfflinePageInfoBarDelegate::Create(base::OnceClosure confirm_continuation,
                                        const GURL& page_to_download,
                                        bool exists_duplicate_request,
                                        content::WebContents* web_contents) {
  // The URL could be very long, especially since we are including query
  // parameters, path, etc.  Elide the URL to a shorter length because the
  // infobar cannot handle scrolling and completely obscures Chrome if the text
  // is too long.
  //
  // 150 was chosen as it does not cause the infobar to overrun the screen on a
  // test Android One device with 480 x 854 resolution.  At this resolution the
  // infobar covers approximately 2/3 of the screen, and all controls are still
  // visible.
  //
  // TODO(dewittj): Display something better than an elided URL string in the
  // infobar.
  const size_t kMaxLengthOfDisplayedPageUrl = 150;

  std::u16string formatted_url = url_formatter::FormatUrl(page_to_download);
  std::u16string elided_url;
  gfx::ElideString(formatted_url, kMaxLengthOfDisplayedPageUrl, &elided_url);

  InfoBarService::FromWebContents(web_contents)
      ->AddInfoBar(DuplicateDownloadInfoBar::CreateInfoBar(
          base::WrapUnique(new OfflinePageInfoBarDelegate(
              std::move(confirm_continuation), base::UTF16ToUTF8(elided_url),
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

}  // namespace offline_pages
