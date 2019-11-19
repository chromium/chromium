// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
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
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server_.GetURL("/title1.html"));
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
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
    scoped_feature_->InitAndEnableFeatureWithParameters(
        features::kCertDualVerificationTrialFeature, {{"uma_only", "true"}});
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
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server_.GetURL("/title1.html"));
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
}

IN_PROC_BROWSER_TEST_F(TrialComparisonCertVerifierFeatureEnabledTest,
                       TrialEnabledPrefEnabled) {
  safe_browsing::SetExtendedReportingPref(browser()->profile()->GetPrefs(),
                                          true);

  ASSERT_TRUE(https_test_server_.Start());
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server_.GetURL("/title1.html"));
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
  if (base::FeatureList::IsEnabled(
          net::features::kCertVerifierBuiltinFeature)) {
    // If both the dual cert verifier trial feature and the builtin verifier
    // feature are enabled, the dual cert verifier trial should not be used.
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
    return;
  }
#endif
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
}

#if BUILDFLAG(BUILTIN_CERT_VERIFIER_FEATURE_SUPPORTED)
class TrialComparisonCertVerifierFeatureOverridenByBuiltinVerifierTest
    : public TrialComparisonCertVerifierTest {
 public:
  TrialComparisonCertVerifierFeatureOverridenByBuiltinVerifierTest() {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
    scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_->InitWithFeaturesAndParameters(
        // None of these tests should generate a report, but set the trial to
        // uma_only mode anyway just to be safe.
        {{features::kCertDualVerificationTrialFeature, {{"uma_only", "true"}}},
         // Enable the builtin verifier.
         {net::features::kCertVerifierBuiltinFeature, {}}},
        {});
  }

  ~TrialComparisonCertVerifierFeatureOverridenByBuiltinVerifierTest() override {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
        false);
  }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_;
};

IN_PROC_BROWSER_TEST_F(
    TrialComparisonCertVerifierFeatureOverridenByBuiltinVerifierTest,
    TrialEnabledPrefEnabledBuiltVerifierEnabled) {
  safe_browsing::SetExtendedReportingPref(browser()->profile()->GetPrefs(),
                                          true);

  ASSERT_TRUE(https_test_server_.Start());
  base::HistogramTester histograms;
  ui_test_utils::NavigateToURL(browser(),
                               https_test_server_.GetURL("/title1.html"));
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
  // If both the dual cert verifier trial feature and the builtin verifier
  // feature are enabled, the dual cert verifier trial should not be used.
  histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 0);
}
#endif
