// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_

#include "base/callback.h"
#include "chrome/browser/download/android/duplicate_download_infobar_delegate.h"
#include "components/infobars/core/infobar_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace offline_pages {

// An InfoBarDelegate that appears when a user attempt to save offline pages for
// a URL that is already saved.  This piggy-backs off the Download infobar,
// since the UI should be the same between Downloads and Offline Pages in this
// case.  There are two actions: Create New, and Overwrite.  Since Overwrite is
// not straightforward for offline pages, the behavior is to delete ALL other
// pages that are saved for the given URL, then save the newly requested page.
class OfflinePageInfoBarDelegate
    : public ::android::DuplicateDownloadInfoBarDelegate {
 public:
  // Creates an offline page infobar and a delegate and adds the infobar to the
  // InfoBarService associated with |web_contents|. |page_name| is the name
  // shown for this file in the infobar text.
  static void Create(const base::Closure& confirm_continuation,
                     const GURL& page_to_download,
                     bool exists_duplicate_request,
                     content::WebContents* web_contents);
  ~OfflinePageInfoBarDelegate() override;

 private:
  OfflinePageInfoBarDelegate(const base::Closure& confirm_continuation,
                             const std::string& page_name,
                             const GURL& page_to_download,
                             bool duplicate_request_exists);

  // DuplicateDownloadInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  bool EqualsDelegate(InfoBarDelegate* delegate) const override;
  bool Accept() override;
  bool Cancel() override;
  std::string GetFilePath() const override;
  bool IsOfflinePage() const override;
  std::string GetPageURL() const override;
  bool ShouldExpire(const NavigationDetails& details) const override;
  bool DuplicateRequestExists() const override;
  OfflinePageInfoBarDelegate* AsOfflinePageInfoBarDelegate() override;

  // Continuation called when the user chooses to create a new file.
  base::Closure confirm_continuation_;

  std::string page_name_;
  GURL page_to_download_;
  bool duplicate_request_exists_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageInfoBarDelegate);
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_ANDROID_DOWNLOADS_OFFLINE_PAGE_INFOBAR_DELEGATE_H_
