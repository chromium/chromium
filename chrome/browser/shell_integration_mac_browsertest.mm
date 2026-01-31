// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#import <Foundation/Foundation.h>

#include "base/apple/bundle_locations.h"
#include "base/apple/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace shell_integration {

using ShellIntegrationMacBrowserTest = InProcessBrowserTest;

// Verifies that the Info.plist content matches the expected scheme
// configuration determined by the C++ logic.
//
// Mac Info.plist is generated at build time (by
// build/apple/tweak_info_plist.py), whereas runtime checks (like
// IsDefaultClientForScheme) rely on C++ logic in shell_integration_mac.mm.
//
// This test ensures these two separate implementations remain in sync for the
// build configuration under test (typically Stable or Chromium).
//
// Note: Logic for removing the scheme for non-stable channels (Beta/Dev/Canary)
// happens during signing (chrome/installer/mac/signing/modification.py) and is
// verified by python unit tests
// (chrome/installer/mac/signing/modification_test.py), as this browser test
// cannot easily run against signed/channel-customized builds.
// TODO(crbuig.com/446672134): Fix and re-enable.
IN_PROC_BROWSER_TEST_F(ShellIntegrationMacBrowserTest,
                       DISABLED_InfoPlistUrlSchemeMatches) {
  std::string expected_scheme = GetDirectLaunchUrlScheme();

  NSBundle* bundle = base::apple::OuterBundle();
  NSArray* url_types = [bundle objectForInfoDictionaryKey:@"CFBundleURLTypes"];

  bool found = false;
  for (NSDictionary* url_type in url_types) {
    NSArray* schemes = url_type[@"CFBundleURLSchemes"];
    for (NSString* scheme in schemes) {
      if (base::SysNSStringToUTF8(scheme) == expected_scheme) {
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }

  if (expected_scheme.empty()) {
    // If we expect no scheme, we should NOT find "google-chrome" or "chromium"
    // registered as a direct launch URL scheme.

    // Explicitly check that we don't have "google-chrome" registered if we
    // expected empty.
    bool found_google_chrome = false;
    for (NSDictionary* url_type in url_types) {
      NSArray* schemes = url_type[@"CFBundleURLSchemes"];
      for (NSString* scheme in schemes) {
        if ([scheme isEqualToString:@"google-chrome"]) {
          found_google_chrome = true;
          break;
        }
      }
    }
    EXPECT_FALSE(found_google_chrome)
        << "Found google-chrome scheme when none was expected.";

    // Check that the placeholder is gone
    bool found_placeholder = false;
    for (NSDictionary* url_type in url_types) {
      NSArray* schemes = url_type[@"CFBundleURLSchemes"];
      for (NSString* scheme in schemes) {
        if ([scheme isEqualToString:@"DIRECT_LAUNCH_URL_SCHEME_PLACEHOLDER"]) {
          found_placeholder = true;
          break;
        }
      }
    }
    EXPECT_FALSE(found_placeholder)
        << "Placeholder scheme still present in Info.plist";

  } else {
    EXPECT_TRUE(found) << "Expected scheme " << expected_scheme
                       << " not found in Info.plist";
  }
}

}  // namespace shell_integration
