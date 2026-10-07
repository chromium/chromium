// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/test_support/fake_web_contents_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

class GlicGuestObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kGlicURLConfig,
          {{features::kGlicGuestURL.name, "https://gemini.google.com"}}},
         {features::kGlicCSPConfig,
          {{features::kGlicAllowedOriginsOverride.name, ""},
           {features::kGlicApiAllowedOrigins.name, ""}}}},
        {});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GlicGuestObserverTest, GrantsAutoplayForGuestOrigin) {
  base::HistogramTester histogram_tester;
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents());
  navigation->ReadyToCommit();
  GrantAutoplayPermissions(navigation->GetNavigationHandle());
  navigation->Commit();

  // Expect WebViewAutoPlayProgress::kAutoPlayGrantedForPrimaryRFH (1).
  histogram_tester.ExpectUniqueSample("Glic.Host.WebView.AutoPlay", 1, 1);
}

TEST_F(GlicGuestObserverTest, GrantsAutoplayForCorpOrigin) {
  base::HistogramTester histogram_tester;
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.corp.google.com/app"), web_contents());
  navigation->ReadyToCommit();
  GrantAutoplayPermissions(navigation->GetNavigationHandle());
  navigation->Commit();

  // Expect WebViewAutoPlayProgress::kAutoPlayGrantedForPrimaryRFH (1).
  histogram_tester.ExpectUniqueSample("Glic.Host.WebView.AutoPlay", 1, 1);
}

TEST_F(GlicGuestObserverTest, DoesNotGrantAutoplayForDisallowedOrigin) {
  base::HistogramTester histogram_tester;
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://evil.com"), web_contents());
  navigation->ReadyToCommit();
  GrantAutoplayPermissions(navigation->GetNavigationHandle());
  navigation->Commit();

  histogram_tester.ExpectTotalCount("Glic.Host.WebView.AutoPlay", 0);
}

TEST_F(GlicGuestObserverTest, DoesNotGrantAutoplayForSubframe) {
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  base::HistogramTester histogram_tester;
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://gemini.google.com/subframe"), subframe);
  subframe_simulator->ReadyToCommit();
  GrantAutoplayPermissions(subframe_simulator->GetNavigationHandle());
  subframe_simulator->Commit();

  histogram_tester.ExpectTotalCount("Glic.Host.WebView.AutoPlay", 0);
}

TEST_F(GlicGuestObserverTest, RecordsMetricsWhenNoWebviewEnabled) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);

  // Subsequent navigation or reload must not record again.
  content::NavigationSimulator::Reload(web_contents());
  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);
}

TEST_F(GlicGuestObserverTest, DoesNotRecordMetricsWhenNoWebviewDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 0);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 0);
}

TEST_F(GlicGuestObserverTest, DoesNotRecordMetricsForSubframeNavigation) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  auto subframe_simulator =
      content::NavigationSimulator::CreateRendererInitiated(
          GURL("https://gemini.google.com/subframe"), subframe);
  subframe_simulator->Commit();

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 0);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 0);
}

TEST_F(GlicGuestObserverTest, DoesNotRecordMetricsForSameDocument) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  // Initial cross-document navigation.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));
  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);

  // Same-document navigation (e.g. fragment link).
  auto same_doc = content::NavigationSimulator::CreateRendererInitiated(
      GURL("https://gemini.google.com/app#hash"), main_rfh());
  same_doc->CommitSameDocument();

  // Counts should remain 1.
  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);
}

TEST_F(GlicGuestObserverTest,
       DoesNotRecordMetricsForErrorPageAndRecoversOnSuccessfulReload) {
  base::test::ScopedFeatureList feature_list(features::kGlicNoWebview);
  FakeWebContentsManager contents_manager;
  GlicGuestObserver::CreateForWebContents(*web_contents(), contents_manager);

  base::HistogramTester histogram_tester;
  // Simulate navigation failing and committing an error page.
  auto failed_nav = content::NavigationSimulator::CreateBrowserInitiated(
      GURL("https://gemini.google.com/app"), web_contents());
  failed_nav->Fail(net::ERR_CONNECTION_FAILED);
  failed_nav->CommitErrorPage();

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 0);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 0);

  // Subsequent successful navigation on reload should be recorded.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://gemini.google.com/app"));

  histogram_tester.ExpectTotalCount("Glic.Contents.NavigationCommitTime", 1);
  histogram_tester.ExpectTotalCount("Glic.Contents.LoadCompleteTime", 1);
}

}  // namespace
}  // namespace glic
