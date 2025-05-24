// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_browsertest_platform_delegate.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace extensions {

ExtensionBrowserTestPlatformDelegate::ExtensionBrowserTestPlatformDelegate(
    ExtensionBrowserTest& parent)
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
    content::WebContents* web_contents = parent_->GetActiveWebContents();
    content::TestNavigationObserver observer(web_contents);
    // The return value is ignored because some tests load URLs that cause
    // redirects, which make NavigateToURL return false. It returns true only if
    // the load succeeds and the final URL matches the passed `url`. Instead we
    // rely on the TestNavigationObserver above to ensure the navigation
    // successfully committed.
    (void)content::NavigateToURL(web_contents, url);
    // Ensure the navigation happened.
    observer.Wait();
    ASSERT_TRUE(observer.last_navigation_succeeded());
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
