// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/cert/trial_comparison_cert_verifier.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

class TrialComparisonCertVerifierTest : public InProcessBrowserTest {
 public:
  TrialComparisonCertVerifierTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

 protected:
  net::EmbeddedTestServer https_test_server_;
};

IN_PROC_BROWSER_TEST_F(TrialComparisonCertVerifierTest, TrialDisabled) {
  ASSERT_TRUE(https_test_server_.Start());
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("/title1.html")));
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
}

class TrialComparisonCertVerifierFeatureEnabledTest
    : public TrialComparisonCertVerifierTest {
 public:
  TrialComparisonCertVerifierFeatureEnabledTest() {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
    scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
    // None of these tests should generate a report, but set the trial to
    // uma_only mode anyway just to be safe.
    scoped_feature_->InitWithFeaturesAndParameters(
        /*enabled_features=*/{{net::features::kCertDualVerificationTrialFeature,
                               {{"uma_only", "true"}}}},
        // This test suite tests enabling the TrialComparisonCertVerifier,
        // which can only be done when KChromeRootStoreUsed is not enabled.
        // There are separate tests below
        // (TrialComparisonCertVerifierFeatureOverridenBy*) for testing that
        // the TrialComparisonCertVerifier is not used when that feature is
        // enabled.
        /*disabled_features=*/{
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
          net::features::kChromeRootStoreUsed,
#endif
        });
  }

  ~TrialComparisonCertVerifierFeatureEnabledTest() override {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
        false);
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_;
};

IN_PROC_BROWSER_TEST_F(TrialComparisonCertVerifierFeatureEnabledTest,
                       TrialEnabledPrefDisabled) {
  ASSERT_TRUE(https_test_server_.Start());
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("/title1.html")));
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
}

IN_PROC_BROWSER_TEST_F(TrialComparisonCertVerifierFeatureEnabledTest,
                       TrialEnabledPrefEnabled) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);

  ASSERT_TRUE(https_test_server_.Start());
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("/title1.html")));
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
}

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
class TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest
    : public TrialComparisonCertVerifierTest {
 public:
  TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest() {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
    scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_->InitWithFeaturesAndParameters(
        // None of these tests should generate a report, but set the trial to
        // uma_only mode anyway just to be safe.
        {{net::features::kCertDualVerificationTrialFeature,
          {{"uma_only", "true"}}},
         // Enable the Chrome Root Store.
         {net::features::kChromeRootStoreUsed, {}}},
        {});
  }

  ~TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest() override {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
        false);
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_;
};

IN_PROC_BROWSER_TEST_F(
    TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest,
    TrialEnabledPrefEnabledBuiltVerifierEnabled) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);

  ASSERT_TRUE(https_test_server_.Start());
  base::HistogramTester histograms;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_test_server_.GetURL("/title1.html")));
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  // If both the dual cert verifier trial feature and the Chrome Root Store
  // feature are enabled, the dual cert verifier trial should not be used.
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
}
#endif  // BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
