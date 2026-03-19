// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/autoplay_policy_status_observer.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutoplayPolicyStatusObserverTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutoplayPolicyStatusObserverTest() = default;
  ~AutoplayPolicyStatusObserverTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    AutoplayPolicyStatusObserver::CreateForWebContents(web_contents());
  }

  AutoplayPolicyStatusObserver* GetObserver() {
    return AutoplayPolicyStatusObserver::FromWebContents(web_contents());
  }
};

TEST_F(AutoplayPolicyStatusObserverTest, RecordsMetricsOnPlay) {
  base::HistogramTester histogram_tester;

  GetObserver()->SetPolicyStatus(
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByEnterprisePolicy);

  // Trigger media started playing.
  content::WebContentsObserver::MediaPlayerInfo player_info(
      false /* has_video */, false /* has_audio */);
  content::MediaPlayerId player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);

  GetObserver()->MediaStartedPlaying(player_info, player_id);

  histogram_tester.ExpectUniqueSample(
      "Media.Autoplay.PolicyStatus",
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByEnterprisePolicy,
      1);
  histogram_tester.ExpectUniqueSample("Media.Autoplay.EnterprisePolicyOverride",
                                      true, 1);
}

TEST_F(AutoplayPolicyStatusObserverTest, RecordsMediaEngagement) {
  base::HistogramTester histogram_tester;

  GetObserver()->SetPolicyStatus(
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByMediaEngagement);

  content::WebContentsObserver::MediaPlayerInfo player_info(
      false /* has_video */, false /* has_audio */);
  content::MediaPlayerId player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);

  GetObserver()->MediaStartedPlaying(player_info, player_id);

  histogram_tester.ExpectUniqueSample(
      "Media.Autoplay.PolicyStatus",
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByMediaEngagement, 1);
  histogram_tester.ExpectUniqueSample("Media.Autoplay.EnterprisePolicyOverride",
                                      false, 1);
}

TEST_F(AutoplayPolicyStatusObserverTest, RecordsWouldBeMediaEngagement) {
  base::HistogramTester histogram_tester;

  GetObserver()->SetPolicyStatus(AutoplayPolicyStatusObserver::PolicyStatus::
                                     kWouldBeAllowedByMediaEngagement);

  content::WebContentsObserver::MediaPlayerInfo player_info(
      false /* has_video */, false /* has_audio */);
  content::MediaPlayerId player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);

  GetObserver()->MediaStartedPlaying(player_info, player_id);

  histogram_tester.ExpectUniqueSample(
      "Media.Autoplay.PolicyStatus",
      AutoplayPolicyStatusObserver::PolicyStatus::
          kWouldBeAllowedByMediaEngagement,
      1);
  histogram_tester.ExpectUniqueSample("Media.Autoplay.EnterprisePolicyOverride",
                                      false, 1);
}

TEST_F(AutoplayPolicyStatusObserverTest, RecordsOnlyOnce) {
  base::HistogramTester histogram_tester;

  GetObserver()->SetPolicyStatus(
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByUserPreference);

  content::WebContentsObserver::MediaPlayerInfo player_info(
      false /* has_video */, false /* has_audio */);
  content::MediaPlayerId player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);

  GetObserver()->MediaStartedPlaying(player_info, player_id);
  GetObserver()->MediaStartedPlaying(player_info, player_id);

  histogram_tester.ExpectUniqueSample(
      "Media.Autoplay.PolicyStatus",
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByUserPreference, 1);
  histogram_tester.ExpectUniqueSample("Media.Autoplay.EnterprisePolicyOverride",
                                      false, 1);
}

TEST_F(AutoplayPolicyStatusObserverTest, ResetsStateOnNavigation) {
  base::HistogramTester histogram_tester;

  // Setup the initial page load and policy.
  GetObserver()->SetPolicyStatus(
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByEnterprisePolicy);

  content::WebContentsObserver::MediaPlayerInfo player_info(
      /*has_video=*/false, /*has_audio=*/false);
  content::MediaPlayerId player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);

  // First media play triggers the metric.
  GetObserver()->MediaStartedPlaying(player_info, player_id);
  histogram_tester.ExpectBucketCount(
      "Media.Autoplay.PolicyStatus",
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByEnterprisePolicy,
      1);

  // Subsequent media play on the same page should be ignored.
  GetObserver()->MediaStartedPlaying(player_info, player_id);
  histogram_tester.ExpectTotalCount("Media.Autoplay.PolicyStatus", 1);

  // Simulate a navigation to a new page.
  NavigateAndCommit(GURL("https://example.com"));
  GetObserver()->SetPolicyStatus(
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByUserPreference);
  content::MediaPlayerId new_player_id(
      web_contents()->GetPrimaryMainFrame()->GetGlobalId(), 0);

  // Media play triggers the metric.
  GetObserver()->MediaStartedPlaying(player_info, new_player_id);

  // Verify the new policy was logged.
  histogram_tester.ExpectBucketCount(
      "Media.Autoplay.PolicyStatus",
      AutoplayPolicyStatusObserver::PolicyStatus::kAllowedByUserPreference, 1);
  histogram_tester.ExpectTotalCount("Media.Autoplay.PolicyStatus", 2);
}
