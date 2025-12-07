// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_android.h"
#else
#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_desktop.h"
#endif

namespace safe_browsing {

// static
std::unique_ptr<DownloadProtectionDelegate>
DownloadProtectionDelegate::CreateForPlatform() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DownloadProtectionDelegateAndroid>();
#else
  return std::make_unique<DownloadProtectionDelegateDesktop>();
#endif
}

std::vector<PendingClientDownloadRequestModification>
DownloadProtectionDelegate::ProduceClientDownloadRequestModifications(
    const download::DownloadItem* item,
    Profile* profile) {
  return std::vector<PendingClientDownloadRequestModification>();
}

}  // namespace safe_browsing
