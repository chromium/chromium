// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_browsertest_platform_delegate.h"
#include "chrome/test/base/ui_test_utils.h"

namespace extensions {

ExtensionBrowserTestPlatformDelegate::ExtensionBrowserTestPlatformDelegate(
    ExtensionBrowserTest& parent)
    : parent_(parent) {}

void ExtensionBrowserTestPlatformDelegate::OpenURL(const GURL& url,
                                                   bool open_in_incognito) {
  if (open_in_incognito) {
    parent_->OpenURLOffTheRecord(parent_->profile(), url);
  } else {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(parent_->browser(), url));
  }
}

}  // namespace extensions
