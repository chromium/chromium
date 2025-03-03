// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate.h"

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_desktop.h"

namespace safe_browsing {

// static
std::unique_ptr<DownloadProtectionDelegate>
DownloadProtectionDelegate::CreateForPlatform() {
  // Temporarily creates a Desktop delegate unconditionally.
  // TODO(crbug.com/397407934): Implement and use the proper delegate for
  // Android.
  return std::make_unique<DownloadProtectionDelegateDesktop>();
}

}  // namespace safe_browsing
