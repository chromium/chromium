// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/duplicate_download_infobar_delegate.h"

namespace android {

bool DuplicateDownloadInfoBarDelegate::IsOfflinePage() const {
  return false;
}

std::string DuplicateDownloadInfoBarDelegate::GetPageURL() const {
  return std::string();
}

std::optional<Profile::OTRProfileID>
DuplicateDownloadInfoBarDelegate::GetOTRProfileID() const {
  return std::nullopt;
}

bool DuplicateDownloadInfoBarDelegate::DuplicateRequestExists() const {
  return false;
}

std::u16string DuplicateDownloadInfoBarDelegate::GetMessageText() const {
  return std::u16string();
}

bool DuplicateDownloadInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

}  // namespace android
