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

  void SetUpOnMainThread() override {
    FingerprintingProtectionFilterBrowserTest::SetUpOnMainThread();
    // Reset the source directory for sample files.
    embedded_test_server()->ServeFilesFromSourceDirectory("chrome/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(FingerprintingProtectionFilterAndroidBrowserTest,
                       MainFrameActivation) {
  GURL url(GetTestUrl("fingerprinting_protection/frame_with_included_script.html"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithPathSuffix("included_script.js"));
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));

  // The root frame document should never be filtered.
  SetRulesetToDisallowURLsWithPathSuffix("frame_with_included_script.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents(), url));
  EXPECT_TRUE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
}

}  // namespace fingerprinting_protection_filter
