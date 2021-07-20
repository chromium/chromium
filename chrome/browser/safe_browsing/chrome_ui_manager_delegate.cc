// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_ui_manager_delegate.h"

#include "chrome/browser/browser_process.h"

namespace safe_browsing {

ChromeSafeBrowsingUIManagerDelegate::ChromeSafeBrowsingUIManagerDelegate() =
    default;
ChromeSafeBrowsingUIManagerDelegate::~ChromeSafeBrowsingUIManagerDelegate() =
    default;

const std::string& ChromeSafeBrowsingUIManagerDelegate::GetApplicationLocale() {
  return g_browser_process->GetApplicationLocale();
}

}  // namespace safe_browsing
