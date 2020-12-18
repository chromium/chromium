// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/login_detection/login_detection_tab_helper.h"
#include "chrome/browser/login_detection/login_detection_type.h"
#include "chrome/browser/login_detection/login_detection_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/site_isolation/features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace login_detection {

class LoginDetectionBrowserTest : public InProcessBrowserTest {
 public:
  LoginDetectionBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{kLoginDetection, {}},
         {site_isolation::features::kSiteIsolationForPasswordSites, {}}},
        {});
  }

  void SetUpOnMainThread() override {
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
    ASSERT_TRUE(embedded_test_server()->Start());
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void ResetHistogramTester() {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  // Verifies the histograms for the given login detection type to be recorded.
  void ExpectLoginDetectionTypeMetric(LoginDetectionType type) {
    histogram_tester_->ExpectUniqueSample("Login.PageLoad.DetectionType", type,
                                          1);
  }

 protected:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that sites saved manual passworded list are detected correctly.
IN_PROC_BROWSER_TEST_F(LoginDetectionBrowserTest,
                       NavigateToManualPasswordedSite) {
  GURL test_url(
      embedded_test_server()->GetURL("www.saved.com", "/title1.html"));

  // Initial navigation will not be treated as no login.
  ui_test_utils::NavigateToURL(browser(), test_url);
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kNoLogin);

  // Use site-isolaiton to save the site to manual passworded list.
  content::SiteInstance::StartIsolatingSite(browser()->profile(), test_url);

  // Subsequent navigation be detected as login.
  ResetHistogramTester();
  ui_test_utils::NavigateToURL(browser(), test_url);
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kPasswordEnteredLogin);

  // Navigations to other subdomains of saved.com are treated as login too.
  ResetHistogramTester();
  ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("mobile.saved.com", "/title1.html"));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kPasswordEnteredLogin);

  ResetHistogramTester();
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("saved.com", "/title1.html"));
  ExpectLoginDetectionTypeMetric(LoginDetectionType::kPasswordEnteredLogin);
}

}  // namespace login_detection
