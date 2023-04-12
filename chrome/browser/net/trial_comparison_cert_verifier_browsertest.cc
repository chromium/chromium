// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/interstitials/security_interstitial_page_test_utils.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/ssl_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "net/base/features.h"
#include "net/cert/trial_comparison_cert_verifier.h"
#include "net/cert/trial_comparison_cert_verifier_util.h"
#include "net/cert/x509_util.h"
#include "net/net_buildflags.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom.h"

// TODO(https://crbug.com/1431334): convert this to a PlatformBrowserTest so
// that it can run on Android too.
class TrialComparisonCertVerifierTest : public InProcessBrowserTest {
 public:
  TrialComparisonCertVerifierTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

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
#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
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

IN_PROC_BROWSER_TEST_F(TrialComparisonCertVerifierFeatureEnabledTest,
                       TrialEnabledPrefEnabledCRLSetUpdate) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);

  ASSERT_TRUE(https_test_server_.Start());
  {
    base::HistogramTester histograms;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_test_server_.GetURL("/title1.html")));
    ASSERT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
        GetActiveWebContents()));

    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                                1);
    histograms.ExpectUniqueSample("Net.CertVerifier_TrialComparisonResult",
                                  net::TrialComparisonResult::kEqual, 1);
  }

  // Apply a CRLSet update that will block the test server cert chain.
  std::string crl_set_bytes;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::ReadFileToString(net::GetTestCertsDirectory().AppendASCII(
                               "crlset_blocked_interception_by_root.raw"),
                           &crl_set_bytes);
  }
  base::RunLoop run_loop;
  content::GetCertVerifierServiceFactory()->UpdateCRLSet(
      base::as_bytes(base::make_span(crl_set_bytes)), run_loop.QuitClosure());
  run_loop.Run();

  {
    base::HistogramTester histograms;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_test_server_.GetURL("/title1.html")));
    // Navigation should be blocked and showing an interstitial. This confirms
    // that the CRLSet update was passed through to the primary verifier.
    ASSERT_TRUE(
        chrome_browser_interstitials::IsShowingBlockedInterceptionInterstitial(
            GetActiveWebContents()));
    ssl_test_util::CheckAuthenticationBrokenState(
        GetActiveWebContents(),
        net::CERT_STATUS_KNOWN_INTERCEPTION_BLOCKED | net::CERT_STATUS_REVOKED,
        ssl_test_util::AuthState::SHOWING_INTERSTITIAL);

    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary", 1);
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                                1);
    // The CRLSet should also be passed to the secondary verifier, so
    // the comparison result should be equal.
    histograms.ExpectUniqueSample("Net.CertVerifier_TrialComparisonResult",
                                  net::TrialComparisonResult::kEqual, 1);
  }
}

#if BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
class TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest
    : public TrialComparisonCertVerifierTest,
      public testing::WithParamInterface<bool> {
 public:
  TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest() {
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(true);
    // This test puts a test cert in the Chrome Root Store, which will fail in
    // builds where Certificate Transparency is required, so disable CT
    // during this test.
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        false);

    // These tests should generate a report, set the trial to uma_only mode so
    // that it won't actually try to send it.
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {net::features::kCertDualVerificationTrialFeature,
         {{"uma_only", "true"}}}};

    std::vector<base::test::FeatureRef> disabled_features;

    if (CRSFeatureStartsEnabled()) {
      enabled_features.push_back({net::features::kChromeRootStoreUsed, {}});
    } else {
      disabled_features.push_back(net::features::kChromeRootStoreUsed);
    }

    scoped_feature_ = std::make_unique<base::test::ScopedFeatureList>();
    scoped_feature_->InitWithFeaturesAndParameters(enabled_features,
                                                   disabled_features);
  }

  ~TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest() override {
    SystemNetworkContextManager::SetEnableCertificateTransparencyForTesting(
        absl::nullopt);
    TrialComparisonCertVerifierController::SetFakeOfficialBuildForTesting(
        false);
  }

  bool CRSFeatureStartsEnabled() const { return GetParam(); }

 protected:
  std::unique_ptr<base::test::ScopedFeatureList> scoped_feature_;
};

IN_PROC_BROWSER_TEST_P(
    TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest,
    TrialEnabledPrefEnabledBuiltVerifierEnabled) {
  safe_browsing::SetExtendedReportingPrefForTests(
      browser()->profile()->GetPrefs(), true);

  // Use a runtime generated cert, as the pre-generated ok_cert has too long of
  // a validity period to be accepted by a publicly trusted root.
  https_test_server_.SetSSLConfig(
      net::test_server::EmbeddedTestServer::CERT_AUTO);
  ASSERT_TRUE(https_test_server_.Start());

  {
    // Create updated Chrome Root Store with just the test server root cert.
    chrome_root_store::RootStore root_store_proto;
    root_store_proto.set_version_major(net::CompiledChromeRootStoreVersion() +
                                       1);

    chrome_root_store::TrustAnchor* anchor =
        root_store_proto.add_trust_anchors();
    scoped_refptr<net::X509Certificate> root_cert =
        net::ImportCertFromFile(net::EmbeddedTestServer::GetRootCertPemPath());
    ASSERT_TRUE(root_cert);
    anchor->set_der(std::string(
        net::x509_util::CryptoBufferAsStringPiece(root_cert->cert_buffer())));

    std::string proto_serialized;
    root_store_proto.SerializeToString(&proto_serialized);
    cert_verifier::mojom::ChromeRootStorePtr root_store_ptr =
        cert_verifier::mojom::ChromeRootStore::New(
            base::as_bytes(base::make_span(proto_serialized)));

    base::RunLoop update_run_loop;
    content::GetCertVerifierServiceFactory()->UpdateChromeRootStore(
        std::move(root_store_ptr), update_run_loop.QuitClosure());
    update_run_loop.Run();
  }

  // Clear test roots so that cert validation only happens with
  // what's in the relevant root store.
  net::TestRootCerts::GetInstance()->Clear();

  // Now verifying the test server certificate should fail with the primary
  // verifier and succeed with the trial verifier since only the trial verifier
  // uses the RotStore update which contains the test cert.

  {
    base::HistogramTester histograms;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_test_server_.GetURL("/title1.html")));
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);

    if (CRSFeatureStartsEnabled()) {
      // Should have loaded successfully.
      EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
          chrome_test_utils::GetActiveWebContents(this)));
      // If both the dual cert verifier trial feature and the Chrome Root Store
      // feature are enabled, the dual cert verifier trial should not be done.
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary",
                                  0);
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                                  0);
    } else {
      // Should not have loaded and should show an SSL interstitial.
      EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
          chrome_test_utils::GetActiveWebContents(this)));
      // Trial should have been run.
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary",
                                  1);
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                                  1);
    }
  }

  const bool new_crs_state = !CRSFeatureStartsEnabled();
  {
    base::RunLoop run_loop;
    content::GetCertVerifierServiceFactory()->SetUseChromeRootStore(
        new_crs_state, run_loop.QuitClosure());
    run_loop.Run();
  }

  {
    base::HistogramTester histograms;
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_test_server_.GetURL("/title2.html")));
    metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
    histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency", 1);

    if (new_crs_state) {
      // Should have loaded successfully.
      EXPECT_FALSE(chrome_browser_interstitials::IsShowingInterstitial(
          chrome_test_utils::GetActiveWebContents(this)));
      // If both the dual cert verifier trial feature and the Chrome Root Store
      // feature are enabled, the dual cert verifier trial should not be done.
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary",
                                  0);
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                                  0);
    } else {
      // Should not have loaded and should show an SSL interstitial.
      EXPECT_TRUE(chrome_browser_interstitials::IsShowingInterstitial(
          chrome_test_utils::GetActiveWebContents(this)));
      // Trial should have been run.
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialPrimary",
                                  1);
      histograms.ExpectTotalCount("Net.CertVerifier_Job_Latency_TrialSecondary",
                                  1);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TrialComparisonCertVerifierFeatureOverridenByChromeRootStoreTest,
    ::testing::Bool());
#endif  // BUILDFLAG(CHROME_ROOT_STORE_OPTIONAL)
