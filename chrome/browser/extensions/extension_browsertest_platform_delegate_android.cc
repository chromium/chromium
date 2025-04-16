// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest_platform_delegate.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/extension_platform_browsertest.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test_utils.h"

namespace extensions {

ExtensionBrowserTestPlatformDelegate::ExtensionBrowserTestPlatformDelegate(
    ExtensionPlatformBrowserTest& parent)
    : parent_(parent) {}

Profile* ExtensionBrowserTestPlatformDelegate::GetProfile() {
  return chrome_test_utils::GetProfile(&(*parent_));
}

void ExtensionBrowserTestPlatformDelegate::SetUpOnMainThread() {}

void ExtensionBrowserTestPlatformDelegate::OpenURL(const GURL& url,
                                                   bool open_in_incognito) {
  if (open_in_incognito) {
    parent_->PlatformOpenURLOffTheRecord(parent_->profile(), url);
  } else {
    ASSERT_TRUE(content::NavigateToURL(parent_->GetActiveWebContents(), url));
  }
}

const Extension* ExtensionBrowserTestPlatformDelegate::LoadAndLaunchApp(
    const base::FilePath& path,
    bool uses_guest_view) {
  NOTREACHED() << "Cannot use platform apps on desktop android.";
}

bool ExtensionBrowserTestPlatformDelegate::WaitForPageActionVisibilityChangeTo(
    int count) {
  NOTIMPLEMENTED() << "Page action count not yet implemented.";
  return false;
}

}  // namespace extensions
