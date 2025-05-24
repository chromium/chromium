// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/classify_url_navigation_throttle.h"

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/supervised_user/core/common/features.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/prerender_test_util.h"
#include "url/gurl.h"

namespace supervised_user {
namespace {

static const char* kExampleHost = "www.example.com";

class ClassifyUrlNavigationThrottleTest
    : public MixinBasedInProcessBrowserTest {
 public:
  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    MixinBasedInProcessBrowserTest::SetUp();
  }

 protected:
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }
  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }
  supervised_user::KidsManagementApiServerMock& kids_management_api_mock() {
    return supervision_mixin_.api_mock_setup_mixin().api_mock();
  }

  base::HistogramTester histogram_tester_;

 private:
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised,
       .embedded_test_server_options = {.resolver_rules_map_host_list =
                                            "*.example.com"}}};
  content::test::PrerenderTestHelper prerender_helper_{
      base::BindRepeating(&ClassifyUrlNavigationThrottleTest::web_contents,
                          base::Unretained(this))};
};

IN_PROC_BROWSER_TEST_F(ClassifyUrlNavigationThrottleTest,
                       RecordsAllowedOnManualList) {
  GURL allowed_url = embedded_test_server()->GetURL(kExampleHost, "/");
  supervised_user_test_util::SetManualFilterForHost(browser()->profile(),
                                                    kExampleHost,
                                                    /*allowlist=*/true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));

  histogram_tester_.ExpectBucketCount(
      kClassifyUrlThrottleFinalStatusHistogramName,
      static_cast<int>(ClassifyUrlThrottleFinalStatus::kAllowed), 1);
}

IN_PROC_BROWSER_TEST_F(ClassifyUrlNavigationThrottleTest,
                       RecordsThrottledFromManualBlocklist) {
  GURL throttled_url = embedded_test_server()->GetURL(kExampleHost, "/");
  supervised_user_test_util::SetManualFilterForHost(browser()->profile(),
                                                    kExampleHost,
                                                    /*allowlist=*/false);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), throttled_url));

  histogram_tester_.ExpectBucketCount(
      kClassifyUrlThrottleFinalStatusHistogramName,
      static_cast<int>(ClassifyUrlThrottleFinalStatus::kBlocked), 1);
}

IN_PROC_BROWSER_TEST_F(ClassifyUrlNavigationThrottleTest, RecordsAllowedAsync) {
  GURL allowed_url = embedded_test_server()->GetURL(kExampleHost, "/");
  kids_management_api_mock().AllowSubsequentClassifyUrl();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), allowed_url));

  histogram_tester_.ExpectBucketCount(
      kClassifyUrlThrottleFinalStatusHistogramName,
      static_cast<int>(ClassifyUrlThrottleFinalStatus::kAllowed), 1);
}

IN_PROC_BROWSER_TEST_F(ClassifyUrlNavigationThrottleTest,
                       RecordsThrottledAsync) {
  GURL throttled_url = embedded_test_server()->GetURL(kExampleHost, "/");
  kids_management_api_mock().RestrictSubsequentClassifyUrl();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), throttled_url));

  histogram_tester_.ExpectBucketCount(
      kClassifyUrlThrottleFinalStatusHistogramName,
      static_cast<int>(ClassifyUrlThrottleFinalStatus::kBlocked), 1);
}
}  // namespace
}  // namespace supervised_user
