// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/apple/scoped_objc_class_swizzler.h"
#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/base/apple/url_conversions.h"

static int g_open_urls_count = 0;

@interface AppController (GoogleChromeSchemeTest)
- (void)swizzled_openUrlsReplacingNTP:(const std::vector<GURL>&)urls;
@end

@implementation AppController (GoogleChromeSchemeTest)
- (void)swizzled_openUrlsReplacingNTP:(const std::vector<GURL>&)urls {
  g_open_urls_count++;
}
@end

namespace {

using AppControllerGoogleChromeSchemeBrowserTest = InProcessBrowserTest;

// TODO(crbuig.com/446672134): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(AppControllerGoogleChromeSchemeBrowserTest,
                       DISABLED_OpenSchemeUrl) {
  std::string scheme = shell_integration::GetDirectLaunchUrlScheme();
  if (scheme.empty()) {
    // Scheme not supported for this channel (e.g. Beta/Dev/Canary).
    return;
  }

  GURL target_url("http://example.com/");
  std::string scheme_url_str = scheme + "://" + target_url.spec();
  NSURL* scheme_url =
      [NSURL URLWithString:base::SysUTF8ToNSString(scheme_url_str)];

  ui_test_utils::AllBrowserTabAddedWaiter waiter;

  AppController* app_controller = AppController.sharedController;
  [app_controller application:NSApp openURLs:@[ scheme_url ]];

  content::WebContents* new_tab = waiter.Wait();
  EXPECT_EQ(new_tab->GetVisibleURL(), target_url);
}

IN_PROC_BROWSER_TEST_F(AppControllerGoogleChromeSchemeBrowserTest,
                       OpenSchemeUrlInvalid) {
  std::string scheme = shell_integration::GetDirectLaunchUrlScheme();
  if (scheme.empty()) {
    return;
  }

  // Swizzle openUrlsReplacingNTP: to verify it is NOT called.
  g_open_urls_count = 0;
  base::apple::ScopedObjCClassSwizzler swizzler(
      [AppController class], @selector(openUrlsReplacingNTP:),
      @selector(swizzled_openUrlsReplacingNTP:));

  // chrome:// settings is disallowed by ValidateUrl (on Mac).
  GURL target_url("chrome://settings/");
  std::string scheme_url_str = scheme + "://" + target_url.spec();
  NSURL* scheme_url =
      [NSURL URLWithString:base::SysUTF8ToNSString(scheme_url_str)];

  AppController* app_controller = AppController.sharedController;
  [app_controller application:NSApp openURLs:@[ scheme_url ]];

  // Since application:openURLs: calls openUrlsReplacingNTP: synchronously
  // (if it decides to proceed), we can check the count immediately.
  EXPECT_EQ(g_open_urls_count, 0);
}

}  // namespace
