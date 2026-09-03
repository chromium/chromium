// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/interstitials/delayed_interstitial_reporter.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/data_protection/data_protection_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace enterprise_data_protection {

using testing::_;

class DelayedInterstitialReporterTest : public ChromeRenderViewHostTestHarness {
 public:
  DelayedInterstitialReporterTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~DelayedInterstitialReporterTest() override = default;

  void SetUp() override { ChromeRenderViewHostTestHarness::SetUp(); }

  static void Start(content::WebContents* web_contents,
                    DelayedInterstitialReporter::TitleCallback report_callback,
                    bool is_bypassing_interstitial,
                    const std::string& uma_suffix) {
    DelayedInterstitialReporter::Start(web_contents, std::move(report_callback),
                                       is_bypassing_interstitial, uma_suffix);
  }
};

TEST_F(DelayedInterstitialReporterTest, SuccessfulTitleExtraction) {
  base::HistogramTester histogram_tester;
  base::MockCallback<DelayedInterstitialReporter::TitleCallback> mock_callback;

  GURL url("https://example.com");

  Start(web_contents(), mock_callback.Get(), false, "SafeBrowsing");

  EXPECT_CALL(mock_callback, Run("My Test Title"));

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      url, web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  content::WebContentsTester::For(web_contents())->SetTitle(u"My Test Title");
  simulator->Commit();
}

TEST_F(DelayedInterstitialReporterTest, StringTruncationLimit) {
  base::MockCallback<DelayedInterstitialReporter::TitleCallback> mock_callback;

  GURL url("https://example.com");

  Start(web_contents(), mock_callback.Get(), false, "SafeBrowsing");

  std::string long_title(2000, 'A');
  std::string expected_title(1024, 'A');

  EXPECT_CALL(mock_callback, Run(expected_title));

  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      url, web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  content::WebContentsTester::For(web_contents())
      ->SetTitle(base::UTF8ToUTF16(long_title));
  simulator->Commit();
}

TEST_F(DelayedInterstitialReporterTest, PrimaryPageChangedDegradation) {
  base::MockCallback<DelayedInterstitialReporter::TitleCallback> mock_callback;

  GURL url("https://example.com");

  Start(web_contents(), mock_callback.Get(), false, "SafeBrowsing");

  EXPECT_CALL(mock_callback, Run("other.com"));

  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("https://other.com"));
}

TEST_F(DelayedInterstitialReporterTest, WebContentsDestroyedDegradation) {
  base::MockCallback<DelayedInterstitialReporter::TitleCallback> mock_callback;

  auto custom_web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), nullptr);

  Start(custom_web_contents.get(), mock_callback.Get(), false, "SafeBrowsing");

  EXPECT_CALL(mock_callback, Run(""));

  custom_web_contents.reset();
}

TEST_F(DelayedInterstitialReporterTest,
       BypassingInterstitialWaitsForNewNavigation) {
  base::MockCallback<DelayedInterstitialReporter::TitleCallback> mock_callback;

  GURL url("https://example.com");

  // Load the page fully so IsDocumentOnLoadCompletedInPrimaryMainFrame is true
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      url, web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  content::WebContentsTester::For(web_contents())->SetTitle(u"My Test Title");
  simulator->Commit();

  // If is_bypassing_interstitial is true, it shouldn't run inline.
  // timer fires or degradation happens. We can just simulate
  // WebContentsDestroyed to trigger it.

  EXPECT_CALL(mock_callback, Run("My Test Title")).Times(0);

  Start(web_contents(), mock_callback.Get(),
        /*is_bypassing_interstitial=*/true, "UrlFiltering");

  // Now we destroy the webcontents to trigger the fallback, and EXPECT_CALL it
  // there.
  EXPECT_CALL(mock_callback, Run(_)).Times(1);
  DeleteContents();
}

TEST_F(DelayedInterstitialReporterTest, BypassingInterstitialWaitAndNavigate) {
  base::HistogramTester histogram_tester;
  base::MockCallback<DelayedInterstitialReporter::TitleCallback> mock_callback;

  GURL url("https://example.com");

  // Load the page fully so IsDocumentOnLoadCompletedInPrimaryMainFrame is true
  auto simulator = content::NavigationSimulator::CreateRendererInitiated(
      url, web_contents()->GetPrimaryMainFrame());
  simulator->Start();
  content::WebContentsTester::For(web_contents())->SetTitle(u"Security error");
  simulator->Commit();

  EXPECT_CALL(mock_callback, Run("Malicious Title")).Times(1);

  Start(web_contents(), mock_callback.Get(),
        /*is_bypassing_interstitial=*/true, "UrlFiltering");

  auto simulator2 = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://malicious.com"), web_contents()->GetPrimaryMainFrame());
  simulator2->Start();
  content::WebContentsTester::For(web_contents())->SetTitle(u"Malicious Title");
  simulator2->Commit();

  // To trigger DidFinishLoad
  content::WebContentsTester::For(web_contents())
      ->TestDidFinishLoad(GURL("https://malicious.com"));

  histogram_tester.ExpectTotalCount(
      "Enterprise.DelayedReportingInterstitial.Time.UrlFiltering", 1);
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DelayedReportingInterstitial.Timeout.UrlFiltering", false, 1);
}



}  // namespace enterprise_data_protection
