// Copyright 2016 The Chromium Authors. All rights reserved.
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

bool DuplicateDownloadInfoBarDelegate::IsOffTheRecord() const {
  return false;
}

bool DuplicateDownloadInfoBarDelegate::DuplicateRequestExists() const {
  return false;
}

base::string16 DuplicateDownloadInfoBarDelegate::GetMessageText() const {
  return base::string16();
}

bool DuplicateDownloadInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

}  // namespace android
