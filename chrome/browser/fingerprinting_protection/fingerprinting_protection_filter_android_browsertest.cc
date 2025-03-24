// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {

class FingerprintingProtectionFilterAndroidBrowserTest
    : public FingerprintingProtectionFilterBrowserTest {
 public:
  FingerprintingProtectionFilterAndroidBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter,
          {{"activation_level", "enabled"},
           {"performance_measurement_rate", "0.0"}}}},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterAndroidBrowserTest,
                       MainFrameActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_a = GetTestUrl("/frame_with_included_script.html");
  GURL url_b = GetCrossSiteTestUrl("/included_script.js");

  ASSERT_NO_FATAL_FAILURE(SetRulesetToDisallowURLsWithSubstring(
      "suffix-that-does-not-match-anything"));
  ASSERT_TRUE(NavigateToDestination(url_a));
  UpdateIncludedScriptSource(url_b);
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  // Use frame_with_no_subresources.html so the only version of
  // "/included_script.js" navigated to is on domain cross-origin.test.
  ASSERT_TRUE(
      NavigateToDestination(GetTestUrl("/frame_with_no_subresources.html")));
  UpdateIncludedScriptSource(url_b);

  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  // Navigate to about:blank first to avoid reusing the previous ruleset for
  // the next check.
  ASSERT_TRUE(NavigateToDestination(GURL(url::kAboutBlankURL)));
  SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html");
  ASSERT_TRUE(NavigateToDestination(url_a));
  UpdateIncludedScriptSource(url_b);

  // The root frame document should never be filtered.
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
}

}  // namespace fingerprinting_protection_filter
