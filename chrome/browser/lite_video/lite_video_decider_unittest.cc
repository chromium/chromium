// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_decider.h"

#include <map>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_hint.h"
#include "chrome/browser/lite_video/lite_video_switches.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist.h"
#include "components/blocklist/opt_out_blocklist/opt_out_blocklist_delegate.h"
#include "components/blocklist/opt_out_blocklist/opt_out_store.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/content/browser/test_optimization_guide_decider.h"
#include "components/optimization_guide/proto/lite_video_metadata.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

class TestLiteVideoUserBlocklist : public lite_video::LiteVideoUserBlocklist {
 public:
  TestLiteVideoUserBlocklist(
      std::unique_ptr<blocklist::OptOutStore> opt_out_store,
      base::Clock* clock,
      blocklist::OptOutBlocklistDelegate* blocklist_delegate)
      : lite_video::LiteVideoUserBlocklist(std::move(opt_out_store),
                                           clock,
                                           blocklist_delegate) {}

  ~TestLiteVideoUserBlocklist() override = default;

  lite_video::LiteVideoBlocklistReason IsLiteVideoAllowedOnNavigation(
      content::NavigationHandle* navigation_handle) const override {
    return blocklist_reason_;
  }

  void set_blocklist_reason(
      lite_video::LiteVideoBlocklistReason blocklist_reason) {
    blocklist_reason_ = blocklist_reason;
  }

 private:
  lite_video::LiteVideoBlocklistReason blocklist_reason_ =
      lite_video::LiteVideoBlocklistReason::kAllowed;
};

class TestLiteVideoHintCache : public lite_video::LiteVideoHintCache {
 public:
  TestLiteVideoHintCache() = default;
  ~TestLiteVideoHintCache() override = default;
  base::Optional<lite_video::LiteVideoHint> GetHintForNavigationURL(
      const GURL& url) const override {
    auto it = hint_cache_.find(url);
    if (it != hint_cache_.end())
      return it->second;
    return base::nullopt;
  }

  void AddHintForTesting(const GURL& url,
                         const lite_video::LiteVideoHint& hint) {
    hint_cache_.insert(std::make_pair(url, hint));
  }

 private:
  std::map<GURL, lite_video::LiteVideoHint> hint_cache_;
};

class TestOptimizationGuideDecider
    : public optimization_guide::TestOptimizationGuideDecider {
 public:
  TestOptimizationGuideDecider() = default;
  ~TestOptimizationGuideDecider() override = default;

  void RegisterOptimizationTypes(
      const std::vector<optimization_guide::proto::OptimizationType>&
          optimization_types) override {
    registered_optimization_types_ =
        base::flat_set<optimization_guide::proto::OptimizationType>(
            optimization_types.begin(), optimization_types.end());
  }

  // Returns the optimization types registered with the Optimization Guide
  // Decider.
  base::flat_set<optimization_guide::proto::OptimizationType>
  registered_optimization_types() {
    return registered_optimization_types_;
  }

  void CanApplyOptimizationAsync(
      content::NavigationHandle* navigation_handle,
      optimization_guide::proto::OptimizationType optimization_type,
      optimization_guide::OptimizationGuideDecisionCallback callback) override {
    GURL url = navigation_handle->GetURL();

    auto response_iter =
        responses_.find(std::make_tuple(url, optimization_type));
    if (response_iter == responses_.end()) {
      std::move(callback).Run(
          optimization_guide::OptimizationGuideDecision::kFalse,
          optimization_guide::OptimizationMetadata());
      return;
    }

    auto response = response_iter->second;
    std::move(callback).Run(std::get<0>(response), std::get<1>(response));
  }

  void SetResponses(
      std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
               std::tuple<optimization_guide::OptimizationGuideDecision,
                          optimization_guide::OptimizationMetadata>>
          responses) {
    responses_ = responses;
  }

 private:
  // The optimization types that were registered with the Optimization Guide
  // Decider.
  base::flat_set<optimization_guide::proto::OptimizationType>
      registered_optimization_types_;

  std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
           std::tuple<optimization_guide::OptimizationGuideDecision,
                      optimization_guide::OptimizationMetadata>>
      responses_;

  DISALLOW_COPY_AND_ASSIGN(TestOptimizationGuideDecider);
};

class LiteVideoDeciderTest : public ChromeRenderViewHostTestHarness {
 public:
  explicit LiteVideoDeciderTest(bool allow_on_forward_back = false)
      : allow_on_forward_back_(allow_on_forward_back) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        ::features::kLiteVideo,
        {{"allow_on_forward_back", allow_on_forward_back_ ? "true" : "false"}});
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    optimization_guide_decider_ =
        std::make_unique<TestOptimizationGuideDecider>();

    lite_video_decider_ = std::make_unique<lite_video::LiteVideoDecider>(
        nullptr, &test_clock_, nullptr);

    lite_video_decider_->OnEffectiveConnectionTypeChanged(
        net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_4G);
    lite_video_decider_->OnConnectionChanged(
        network::mojom::ConnectionType::CONNECTION_4G);

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        "enable-spdy-proxy-auth");
  }

  void UseOptimizationGuideDecider() {
    optimization_guide_decider_->RegisterOptimizationTypes(
        {optimization_guide::proto::LITE_VIDEO});
    lite_video_decider_->SetOptimizationGuideDeciderForTesting(
        optimization_guide_decider_.get());
  }

  void DisableLiteVideo() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitAndDisableFeature({::features::kLiteVideo});
  }

  void CanApplyOnSubframeNavigation(const GURL& mainframe_url,
                                    const GURL& subframe_url) {
    // Needed so that a mainframe navigation exists.
    NavigateAndCommit(mainframe_url);
    content::RenderFrameHostTester* rfh_tester =
        content::RenderFrameHostTester::For(main_rfh());
    content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
    content::MockNavigationHandle navigation_handle(subframe_url, subframe);
    lite_video_decider()->CanApplyLiteVideo(
        &navigation_handle,
        base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                       base::Unretained(this)));
  }

  void SetDurationFromTimeDelta(optimization_guide::proto::Duration* duration,
                                const base::TimeDelta& delta) {
    if (!duration)
      return;
    duration->set_seconds(delta.InSeconds());
    duration->set_nanos(delta.InNanoseconds() %
                        base::TimeDelta::FromSeconds(1).InNanoseconds());
  }

  void SeedLiteVideoHintCache(const GURL& gurl,
                              base::Optional<lite_video::LiteVideoHint> hint,
                              bool use_opt_guide) {
    if (use_opt_guide) {
      optimization_guide::OptimizationMetadata default_metadata;
      optimization_guide::proto::LiteVideoMetadata lite_video_metadata;
      if (hint) {
        optimization_guide::proto::LiteVideoHint* hint_proto =
            lite_video_metadata.mutable_lite_video_hint();
        hint_proto->set_target_downlink_bandwidth_kbps(
            hint->target_downlink_bandwidth_kbps());
        hint_proto->set_kilobytes_to_buffer_before_throttle(
            hint->kilobytes_to_buffer_before_throttle());
        SetDurationFromTimeDelta(
            hint_proto->mutable_target_downlink_rtt_latency(),
            hint->target_downlink_rtt_latency());
        SetDurationFromTimeDelta(hint_proto->mutable_max_throttling_delay(),
                                 hint->max_throttling_delay());
        default_metadata.SetAnyMetadataForTesting(lite_video_metadata);
      }

      std::map<std::tuple<GURL, optimization_guide::proto::OptimizationType>,
               std::tuple<optimization_guide::OptimizationGuideDecision,
                          optimization_guide::OptimizationMetadata>>
          responses = {
              {std::make_tuple(gurl, optimization_guide::proto::LITE_VIDEO),
               std::make_tuple(
                   optimization_guide::OptimizationGuideDecision::kTrue,
                   default_metadata)},
          };

      optimization_guide_decider_->SetResponses(responses);
      return;
    }
    std::unique_ptr<TestLiteVideoHintCache> hint_cache =
        std::make_unique<TestLiteVideoHintCache>();
    hint_cache->AddHintForTesting(gurl, *hint);
    lite_video_decider_->SetHintCacheForTesting(std::move(hint_cache));
  }

  void SetBlocklistReason(lite_video::LiteVideoBlocklistReason reason) {
    std::unique_ptr<TestLiteVideoUserBlocklist> user_blocklist_ =
        std::make_unique<TestLiteVideoUserBlocklist>(nullptr, &test_clock_,
                                                     lite_video_decider_.get());
    user_blocklist_->set_blocklist_reason(reason);
    lite_video_decider_->SetUserBlocklistForTesting(std::move(user_blocklist_));
  }

  void SeedPermanentHostBlocklist(
      const base::flat_set<std::string>& permanent_host_blocklist) {
    lite_video_decider_->SetPermanentHostBlocklistForTesting(
        permanent_host_blocklist);
  }

  lite_video::LiteVideoDecider* lite_video_decider() {
    return lite_video_decider_.get();
  }

  void OnHintAvailable(
      base::Optional<lite_video::LiteVideoHint> hint,
      lite_video::LiteVideoBlocklistReason blocklist_reason,
      optimization_guide::OptimizationGuideDecision opt_guide_decision) {
    opt_guide_decision_ = opt_guide_decision;
    hint_ = hint;
    blocklist_reason_ = blocklist_reason;
  }

  base::Optional<lite_video::LiteVideoHint> hint() { return hint_; }

  optimization_guide::OptimizationGuideDecision opt_guide_decision() {
    return opt_guide_decision_;
  }

  lite_video::LiteVideoBlocklistReason blocklist_reason() {
    return blocklist_reason_;
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  void RunUntilIdle() {
    task_environment()->RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock test_clock_;
  std::unique_ptr<lite_video::LiteVideoDecider> lite_video_decider_;
  lite_video::LiteVideoBlocklistReason blocklist_reason_;
  base::Optional<lite_video::LiteVideoHint> hint_;
  std::unique_ptr<TestOptimizationGuideDecider> optimization_guide_decider_;
  optimization_guide::OptimizationGuideDecision opt_guide_decision_;
  bool allow_on_forward_back_;
};

TEST_F(LiteVideoDeciderTest, CanApplyOnNonHTTPOrHTTPSURL) {
  base::HistogramTester histogram_tester;

  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("chrome:://about"));
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));

  RunUntilIdle();
  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kUnknown);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", 0);
}

TEST_F(LiteVideoDeciderTest, CanApplyNoHintAndHostBlocklisted) {
  base::HistogramTester histogram_tester;
  SetBlocklistReason(
      lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted);
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://NoVideo.com"));
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));

  RunUntilIdle();
  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(),
            lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationBlocklisted, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
}

TEST_F(LiteVideoDeciderTest, CanApplyAllowedButNoHint) {
  base::HistogramTester histogram_tester;
  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);

  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(GURL("https://NoVideo.com"));
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));

  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
}

TEST_F(LiteVideoDeciderTest, CanApplyLiteVideo) {
  base::HistogramTester histogram_tester;

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/false);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));

  ASSERT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(seeded_hint.target_downlink_bandwidth_kbps(),
            hint()->target_downlink_bandwidth_kbps());
  EXPECT_EQ(seeded_hint.target_downlink_rtt_latency(),
            hint()->target_downlink_rtt_latency());
  EXPECT_EQ(seeded_hint.kilobytes_to_buffer_before_throttle(),
            hint()->kilobytes_to_buffer_before_throttle());
  EXPECT_EQ(seeded_hint.max_throttling_delay(), hint()->max_throttling_delay());
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 1);
}

TEST_F(LiteVideoDeciderTest, LiteVideoDisabled) {
  DisableLiteVideo();
  base::HistogramTester histogram_tester;
  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/false);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kUnknown);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", 0);
}

TEST_F(LiteVideoDeciderTest, LiteVideoCanApplyOnSubframeNavigation) {
  base::HistogramTester histogram_tester;

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/false);

  CanApplyOnSubframeNavigation(GURL("https://mainframe.com"), url);
  ASSERT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(seeded_hint.target_downlink_bandwidth_kbps(),
            hint()->target_downlink_bandwidth_kbps());
  EXPECT_EQ(seeded_hint.target_downlink_rtt_latency(),
            hint()->target_downlink_rtt_latency());
  EXPECT_EQ(seeded_hint.kilobytes_to_buffer_before_throttle(),
            hint()->kilobytes_to_buffer_before_throttle());
  EXPECT_EQ(seeded_hint.max_throttling_delay(), hint()->max_throttling_delay());
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 1);
}

TEST_F(LiteVideoDeciderTest, CanApplyOnReload) {
  base::HistogramTester histogram_tester;

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_RELOAD);

  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/false);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(),
            lite_video::LiteVideoBlocklistReason::kNavigationReload);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationReload, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
}

TEST_F(LiteVideoDeciderTest, CanApplyOnForwardBackNavigation) {
  base::HistogramTester histogram_tester;

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_FORWARD_BACK);

  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/false);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(),
            lite_video::LiteVideoBlocklistReason::kNavigationForwardBack);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kNavigationForwardBack, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
}

TEST_F(LiteVideoDeciderTest, SetDefaultDownlinkBandwidthOverride) {
  base::HistogramTester histogram_tester;
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      lite_video::switches::kLiteVideoDefaultDownlinkBandwidthKbps, "200");
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      lite_video::switches::kLiteVideoForceOverrideDecision);

  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));

  ASSERT_TRUE(hint());
  EXPECT_EQ(200, hint()->target_downlink_bandwidth_kbps());
}

TEST_F(LiteVideoDeciderTest, OptimizationGuide_CanApplyLiteVideo) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/true);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();

  ASSERT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(seeded_hint.target_downlink_bandwidth_kbps(),
            hint()->target_downlink_bandwidth_kbps());
  EXPECT_EQ(seeded_hint.target_downlink_rtt_latency(),
            hint()->target_downlink_rtt_latency());
  EXPECT_EQ(seeded_hint.kilobytes_to_buffer_before_throttle(),
            hint()->kilobytes_to_buffer_before_throttle());
  EXPECT_EQ(seeded_hint.max_throttling_delay(), hint()->max_throttling_delay());
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.LiteVideoDecider.OptGuideHintCacheSize", 1, 1);
}

TEST_F(LiteVideoDeciderTest, OptimizationGuide_NoMetadata_CanApplyLiteVideo) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  SeedLiteVideoHintCache(url, /*hint=*/base::nullopt, /*use_opt_guide=*/true);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();

  ASSERT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(lite_video::switches::GetDefaultDownlinkBandwidthKbps(),
            hint()->target_downlink_bandwidth_kbps());
  EXPECT_EQ(lite_video::features::LiteVideoTargetDownlinkRTTLatency(),
            hint()->target_downlink_rtt_latency());
  EXPECT_EQ(lite_video::features::LiteVideoKilobytesToBufferBeforeThrottle(),
            hint()->kilobytes_to_buffer_before_throttle());
  EXPECT_EQ(lite_video::features::LiteVideoMaxThrottlingDelay(),
            hint()->max_throttling_delay());
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.LiteVideoDecider.OptGuideHintCacheSize", 1, 1);
}

TEST_F(LiteVideoDeciderTest, OptimizationGuide_CanApplyOnSubframeNavigation) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  GURL mainframe_url("https://mainframe.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(mainframe_url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);
  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(mainframe_url, seeded_hint, /*use_opt_guide=*/true);

  // Force a check on the mainframe, otherwise no hint will be set for the
  // subframe.
  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();

  CanApplyOnSubframeNavigation(mainframe_url, url);
  RunUntilIdle();

  ASSERT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kTrue);
  EXPECT_EQ(seeded_hint.target_downlink_bandwidth_kbps(),
            hint()->target_downlink_bandwidth_kbps());
  EXPECT_EQ(seeded_hint.target_downlink_rtt_latency(),
            hint()->target_downlink_rtt_latency());
  EXPECT_EQ(seeded_hint.kilobytes_to_buffer_before_throttle(),
            hint()->kilobytes_to_buffer_before_throttle());
  EXPECT_EQ(seeded_hint.max_throttling_delay(), hint()->max_throttling_delay());
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 2);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.LiteVideoDecider.OptGuideHintCacheSize", 1, 1);
}

TEST_F(LiteVideoDeciderTest,
       OptimizationGuide_CanApplyOnSubframeNavigation_Unknown) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  GURL mainframe_url("https://mainframe.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(mainframe_url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  SeedLiteVideoHintCache(mainframe_url, base::nullopt, /*use_opt_guide=*/true);

  CanApplyOnSubframeNavigation(mainframe_url, url);
  RunUntilIdle();

  EXPECT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kUnknown);

  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.LiteVideoDecider.OptGuideHintCacheSize", 0);
}

TEST_F(LiteVideoDeciderTest,
       OptimizationGuide_CanApplyOnSubframeNavigation_MainframeFalse) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL subframe_url("https://LiteVideo.com");
  GURL mainframe_url("https://mainframe.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(mainframe_url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  SeedLiteVideoHintCache(subframe_url, base::nullopt, /*use_opt_guide=*/true);

  // Force a check on the mainframe, otherwise no hint will be set for the
  // subframe.
  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();
  ASSERT_FALSE(hint());
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kFalse);

  CanApplyOnSubframeNavigation(mainframe_url, subframe_url);
  RunUntilIdle();

  ASSERT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kFalse);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 2);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.LiteVideoDecider.OptGuideHintCacheSize", 1, 1);
}

TEST_F(LiteVideoDeciderTest, OptimizationGuide_HostOnPermanentBlocklist) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();
  SeedPermanentHostBlocklist({"mainframe.com"});

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL mainframe_url("https://mainframe.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(mainframe_url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  SeedLiteVideoHintCache(mainframe_url, base::nullopt, /*use_opt_guide=*/true);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();
  ASSERT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(),
            lite_video::LiteVideoBlocklistReason::kHostPermanentlyBlocklisted);
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kFalse);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kHostPermanentlyBlocklisted, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
}

TEST_F(LiteVideoDeciderTest, OptimizationGuide_PermanentBlocklist_HostAllowed) {
  base::HistogramTester histogram_tester;
  UseOptimizationGuideDecider();
  SeedPermanentHostBlocklist({"otherhost.com"});

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL mainframe_url("https://mainframe.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(mainframe_url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  SeedLiteVideoHintCache(mainframe_url, base::nullopt, /*use_opt_guide=*/true);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();
  ASSERT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kTrue);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 1);
}

TEST_F(LiteVideoDeciderTest, HostOnPermanentBlocklist) {
  base::HistogramTester histogram_tester;
  SeedPermanentHostBlocklist({"mainframe.com"});

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL mainframe_url("https://mainframe.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(mainframe_url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_TYPED);

  SeedLiteVideoHintCache(mainframe_url, base::nullopt, /*use_opt_guide=*/true);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  RunUntilIdle();
  ASSERT_FALSE(hint());
  EXPECT_EQ(blocklist_reason(),
            lite_video::LiteVideoBlocklistReason::kHostPermanentlyBlocklisted);
  EXPECT_EQ(opt_guide_decision(),
            optimization_guide::OptimizationGuideDecision::kFalse);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kHostPermanentlyBlocklisted, 1);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", false, 1);
}

class LiteVideoDeciderAllowOnForwardBackTest : public LiteVideoDeciderTest {
 public:
  LiteVideoDeciderAllowOnForwardBackTest()
      : LiteVideoDeciderTest(/*allow_on_forward_back=*/true) {}
};

TEST_F(LiteVideoDeciderAllowOnForwardBackTest,
       CanApplyOnForwardBackNavigation) {
  base::HistogramTester histogram_tester;

  SetBlocklistReason(lite_video::LiteVideoBlocklistReason::kAllowed);
  GURL url("https://LiteVideo.com");
  content::MockNavigationHandle navigation_handle(web_contents());
  navigation_handle.set_url(url);
  navigation_handle.set_page_transition(ui::PAGE_TRANSITION_FORWARD_BACK);

  lite_video::LiteVideoHint seeded_hint(
      /*target_downlink_bandwidth_kbps=*/123,
      /*target_downlink_rtt_latency=*/base::TimeDelta::FromMilliseconds(2500),
      /*kilobytes_to_buffer_before_throttle=*/500,
      /*max_throttling_delay=*/base::TimeDelta::FromMilliseconds(5000));
  SeedLiteVideoHintCache(url, seeded_hint, /*use_opt_guide=*/false);

  lite_video_decider()->CanApplyLiteVideo(
      &navigation_handle, base::BindOnce(&LiteVideoDeciderTest::OnHintAvailable,
                                         base::Unretained(this)));
  EXPECT_TRUE(hint());
  EXPECT_EQ(blocklist_reason(), lite_video::LiteVideoBlocklistReason::kAllowed);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.MainFrame",
      lite_video::LiteVideoBlocklistReason::kAllowed, 1);
  histogram_tester.ExpectTotalCount(
      "LiteVideo.CanApplyLiteVideo.UserBlocklist.SubFrame", 0);
  histogram_tester.ExpectUniqueSample(
      "LiteVideo.CanApplyLiteVideo.HintCache.HasHint", true, 1);
}
