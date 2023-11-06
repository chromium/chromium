// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/net/browser_url_opener_impl.h"

#include "ash/public/cpp/new_window_delegate.h"

namespace arc {

void BrowserUrlOpenerImpl::OpenUrl(GURL url) {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);
}

}  // namespace arc
