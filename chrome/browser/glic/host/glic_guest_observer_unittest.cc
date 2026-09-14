// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_guest_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/test/navigation_simulator.h"
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

}  // namespace
}  // namespace glic
