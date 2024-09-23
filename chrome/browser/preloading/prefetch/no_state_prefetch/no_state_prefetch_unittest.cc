// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/with_feature_override.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/no_state_prefetch_unit_test_utils.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_field_trial.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_handle.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_link_manager.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/no_state_prefetch/common/no_state_prefetch_origin.h"
#include "components/no_state_prefetch/common/no_state_prefetch_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using base::Time;
using base::TimeTicks;
using content::Referrer;

namespace prerender {

namespace {

class TestNetworkBytesChangedObserver
    : public prerender::NoStatePrefetchHandle::Observer {
 public:
  TestNetworkBytesChangedObserver() = default;

  TestNetworkBytesChangedObserver(const TestNetworkBytesChangedObserver&) =
      delete;
  TestNetworkBytesChangedObserver& operator=(
      const TestNetworkBytesChangedObserver&) = delete;

  // prerender::NoStatePrefetchHandle::Observer
  void OnPrefetchStop(
      NoStatePrefetchHandle* no_state_prefetch_handle) override {}
};

const gfx::Size kDefaultViewSize(640, 480);

}  // namespace

class MockNetworkChangeNotifier4GMetered : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }

  ConnectionCost GetCurrentConnectionCost() override {
    return NetworkChangeNotifier::CONNECTION_COST_METERED;
  }
};

class MockNetworkChangeNotifier4GUnmetered : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }

  ConnectionCost GetCurrentConnectionCost() override {
    return NetworkChangeNotifier::CONNECTION_COST_UNMETERED;
  }
};

class MockNetworkChangeNotifierWifiMetered : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_WIFI;
  }

  ConnectionCost GetCurrentConnectionCost() override {
    return NetworkChangeNotifier::CONNECTION_COST_METERED;
  }
};

class NoStatePrefetchTest : public testing::Test {
 public:
  static const int kDefaultChildId = -1;
  static const int kDefaultRenderViewRouteId = -1;
  static const int kDefaultRenderFrameRouteId = -1;

  NoStatePrefetchTest()
      : no_state_prefetch_manager_(
            new UnitTestNoStatePrefetchManager(&profile_)),
        no_state_prefetch_link_manager_(
            new NoStatePrefetchLinkManager(no_state_prefetch_manager_.get())) {
    no_state_prefetch_manager()->SetIsLowEndDevice(false);
  }

  ~NoStatePrefetchTest() override {
    no_state_prefetch_link_manager_->Shutdown();
    no_state_prefetch_manager_->Shutdown();
  }

  void TearDown() override {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::SimpleTestTickClock* tick_clock() { return &tick_clock_; }

  UnitTestNoStatePrefetchManager* no_state_prefetch_manager() {
    return no_state_prefetch_manager_.get();
  }

  NoStatePrefetchLinkManager* no_state_prefetch_link_manager() {
    return no_state_prefetch_link_manager_.get();
  }

  Profile* profile() { return &profile_; }

  void SetConcurrency(size_t concurrency) {
    no_state_prefetch_manager()
        ->mutable_config()
        .max_link_concurrency_per_launcher = concurrency;
    no_state_prefetch_manager()->mutable_config().max_link_concurrency =
        std::max(
            no_state_prefetch_manager()->mutable_config().max_link_concurrency,
            concurrency);
  }

  bool IsEmptyNoStatePrefetchLinkManager() const {
    return no_state_prefetch_link_manager_->IsEmpty();
  }

  size_t CountExistingTriggers() {
    return no_state_prefetch_link_manager()->triggers_.size();
  }

  bool LastTriggerExists() {
    return no_state_prefetch_link_manager()->triggers_.begin() !=
           no_state_prefetch_link_manager()->triggers_.end();
  }

  bool LastTriggerIsRunning() {
    CHECK(LastTriggerExists());
    return no_state_prefetch_link_manager()->TriggerIsRunningForTesting(
        no_state_prefetch_link_manager()->triggers_.back().get());
  }

  bool AddLinkTrigger(const GURL& url,
                      const GURL& initiator_url,
                      int render_process_id,
                      int render_view_id,
                      int render_frame_id) {
    auto attributes = blink::mojom::PrerenderAttributes::New();
    attributes->url = url;
    attributes->trigger_type =
        blink::mojom::PrerenderTriggerType::kLinkRelPrerender;
    attributes->referrer = blink::mojom::Referrer::New(
        initiator_url, network::mojom::ReferrerPolicy::kDefault);
    attributes->view_size = kDefaultViewSize;

    // This could delete an existing prefetcher as a side-effect.
    std::optional<int> link_trigger_id =
        no_state_prefetch_link_manager()->OnStartLinkTrigger(
            render_process_id, render_view_id, render_frame_id,
            std::move(attributes), url::Origin::Create(initiator_url));

    // Check if the new prefetch request was added and running.
    return link_trigger_id && LastTriggerIsRunning();
  }

  // Shorthand to add a simple link trigger with a reasonable source. Returns
  // true iff the prefetcher has been added to the NoStatePrefetchManager by the
  // NoStatePrefetchLinkManager and the NoStatePrefetchManager returned a
  // handle.
  bool AddSimpleLinkTrigger(const GURL& url) {
    return AddLinkTrigger(url, GURL(), kDefaultChildId,
                          kDefaultRenderViewRouteId,
                          kDefaultRenderFrameRouteId);
  }

  // Shorthand to add a simple link trigger with a reasonable source. Returns
  // true iff the prefetcher has been added to the NoStatePrefetchManager by the
  // NoStatePrefetchLinkManager and the NoStatePrefetchManager returned a
  // handle. The referrer is set to a google domain.
  bool AddSimpleGWSLinkTrigger(const GURL& url) {
    return AddLinkTrigger(url, GURL("https://www.google.com"), kDefaultChildId,
                          kDefaultRenderViewRouteId,
                          kDefaultRenderFrameRouteId);
  }

  void AbandonFirstTrigger() {
    CHECK(!no_state_prefetch_link_manager()->triggers_.empty());
    no_state_prefetch_link_manager()->OnAbandonLinkTrigger(
        no_state_prefetch_link_manager()->triggers_.front()->link_trigger_id);
  }

  void AbandonLastTrigger() {
    CHECK(!no_state_prefetch_link_manager()->triggers_.empty());
    no_state_prefetch_link_manager()->OnAbandonLinkTrigger(
        no_state_prefetch_link_manager()->triggers_.back()->link_trigger_id);
  }

  void CancelFirstTrigger() {
    CHECK(!no_state_prefetch_link_manager()->triggers_.empty());
    no_state_prefetch_link_manager()->OnCancelLinkTrigger(
        no_state_prefetch_link_manager()->triggers_.front()->link_trigger_id);
  }

  void CancelLastTrigger() {
    CHECK(!no_state_prefetch_link_manager()->triggers_.empty());
    no_state_prefetch_link_manager()->OnCancelLinkTrigger(
        no_state_prefetch_link_manager()->triggers_.back()->link_trigger_id);
  }

  void DisablePrerender() {
    prefetch::SetPreloadPagesState(profile_.GetPrefs(),
                                   prefetch::PreloadPagesState::kNoPreloading);
  }

  void EnablePrerender() {
    prefetch::SetPreloadPagesState(
        profile_.GetPrefs(), prefetch::PreloadPagesState::kStandardPreloading);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  // This needs to be initialized before any tasks running on other threads
  // access the feature list, and destroyed after |task_environment_|, to avoid
  // data races.
  base::test::ScopedFeatureList feature_list_;

 private:
  // Needed to pass NoStatePrefetchManager's DCHECKs.
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<UnitTestNoStatePrefetchManager> no_state_prefetch_manager_;
  std::unique_ptr<NoStatePrefetchLinkManager> no_state_prefetch_link_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(NoStatePrefetchTest, RespectsThirdPartyCookiesPref) {
  GURL url("http://www.google.com/");
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  histogram_tester().ExpectUniqueSample(
      "Prerender.FinalStatus", FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES, 1);
}

class NoStatePrefetchGWSPrefetchHoldbackTest : public NoStatePrefetchTest {
 public:
  NoStatePrefetchGWSPrefetchHoldbackTest() {
    feature_list_.InitAndEnableFeature(kGWSPrefetchHoldback);
  }
};

TEST_F(NoStatePrefetchGWSPrefetchHoldbackTest,
       GWSPrefetchHoldbackNonGWSSReferrer) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

TEST_F(NoStatePrefetchGWSPrefetchHoldbackTest, GWSPrefetchHoldbackGWSReferrer) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.google.com")), ORIGIN_GWS_PRERENDER,
      FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_FALSE(AddSimpleGWSLinkTrigger(url));
}

class NoStatePrefetchGWSPrefetchHoldbackOffTest : public NoStatePrefetchTest {
 public:
  NoStatePrefetchGWSPrefetchHoldbackOffTest() {
    feature_list_.InitAndDisableFeature(kGWSPrefetchHoldback);
  }
};

TEST_F(NoStatePrefetchGWSPrefetchHoldbackOffTest,
       GWSPrefetchHoldbackOffNonGWSReferrer) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

TEST_F(NoStatePrefetchGWSPrefetchHoldbackOffTest,
       GWSPrefetchHoldbackOffGWSReferrer) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.google.com")), ORIGIN_GWS_PRERENDER,
      FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleGWSLinkTrigger(url));
}

class PrerendererNavigationPredictorPrefetchHoldbackTest
    : public NoStatePrefetchTest {
 public:
  PrerendererNavigationPredictorPrefetchHoldbackTest() {
    feature_list_.InitAndEnableFeature(kNavigationPredictorPrefetchHoldback);
  }
};

TEST_F(PrerendererNavigationPredictorPrefetchHoldbackTest,
       PredictorPrefetchHoldbackNonPredictorReferrer) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.notgoogle.com")),
      ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

// Verify that link-rel:next URLs are not prefetched.
TEST_F(NoStatePrefetchTest, LinkRelNextWithNSPDisabled) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.notgoogle.com")), ORIGIN_LINK_REL_NEXT,
      FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_EQ(nullptr, no_state_prefetch_manager()
                         ->StartPrefetchingWithPreconnectFallbackForTesting(
                             ORIGIN_LINK_REL_NEXT, url,
                             url::Origin::Create(GURL("www.notgoogle.com"))));
  histogram_tester().ExpectUniqueSample(
      "Prerender.FinalStatus", FINAL_STATUS_LINK_REL_NEXT_NOT_ALLOWED, 1);
}

class PrerendererNavigationPredictorPrefetchHoldbackDisabledTest
    : public NoStatePrefetchTest {
 public:
  PrerendererNavigationPredictorPrefetchHoldbackDisabledTest() {
    feature_list_.InitAndDisableFeature(kNavigationPredictorPrefetchHoldback);
  }
};

TEST_F(PrerendererNavigationPredictorPrefetchHoldbackDisabledTest,
       PredictorPrefetchHoldbackOffNonPredictorReferrer) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.notgoogle.com")),
      ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

// Flaky on Android and Mac, crbug.com/1087876.
TEST_F(NoStatePrefetchTest, DISABLED_PrerenderDisabledOnLowEndDevice) {
  GURL url("http://www.google.com/");
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_LOW_END_DEVICE, 1);
}

TEST_F(NoStatePrefetchTest, FoundTest) {
  base::TimeDelta prefetch_age;
  FinalStatus final_status;
  Origin origin;

  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());

  GURL url("http://www.google.com/");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());

  EXPECT_TRUE(no_state_prefetch_manager()->GetPrefetchInformation(
      url, &prefetch_age, &final_status, &origin));
  EXPECT_EQ(prerender::FINAL_STATUS_UNKNOWN, final_status);
  EXPECT_EQ(prerender::ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, origin);

  const base::TimeDelta advance_duration = base::Seconds(1);
  tick_clock()->Advance(advance_duration);
  EXPECT_TRUE(no_state_prefetch_manager()->GetPrefetchInformation(
      url, &prefetch_age, &final_status, &origin));
  EXPECT_LE(advance_duration, prefetch_age);
  EXPECT_EQ(prerender::FINAL_STATUS_UNKNOWN, final_status);
  EXPECT_EQ(prerender::ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, origin);

  no_state_prefetch_manager()->ClearPrefetchInformationForTesting();
  EXPECT_FALSE(no_state_prefetch_manager()->GetPrefetchInformation(
      url, &prefetch_age, &final_status, &origin));
}

// Flaky on Android, crbug.com/1088454.
// Make sure that if queue a request, and a second prerender request for the
// same URL comes in, that the second request attaches to the first prerender,
// and we don't use the second prerender contents.
// This test is the same as the "DuplicateTest" above, but for NoStatePrefetch.
TEST_F(NoStatePrefetchTest, DISABLED_DuplicateTest_NoStatePrefetch) {
  SetConcurrency(2);
  GURL url("http://www.google.com/");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());

  FakeNoStatePrefetchContents* no_state_prefetch_contents1 =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_EQ(no_state_prefetch_contents1,
            no_state_prefetch_manager()->next_no_state_prefetch_contents());
  EXPECT_FALSE(no_state_prefetch_contents1->prefetching_has_started());

  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Ensure that we expire a prerendered page after the max. permitted time.
TEST_F(NoStatePrefetchTest, ExpireTest) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  GURL url("http://www.google.com/");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  tick_clock()->Advance(no_state_prefetch_manager()->config().time_to_live +
                        base::Seconds(1));
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Ensure that we don't launch prerenders of bad urls (in this case, a mailto:
// url)
TEST_F(NoStatePrefetchTest, BadURLTest) {
  GURL url("mailto:test@gmail.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_UNSUPPORTED_SCHEME);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// When the user navigates away from a page, the prerenders it launched should
// have their time to expiry shortened from the default time to live.
TEST_F(NoStatePrefetchTest, LinkManagerNavigateAwayExpire) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  const base::TimeDelta time_to_live = base::Seconds(300);
  const base::TimeDelta abandon_time_to_live = base::Seconds(20);
  const base::TimeDelta test_advance = base::Seconds(22);
  ASSERT_LT(test_advance, time_to_live);
  ASSERT_LT(abandon_time_to_live, test_advance);

  no_state_prefetch_manager()->mutable_config().time_to_live = time_to_live;
  no_state_prefetch_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  AbandonLastTrigger();
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
  tick_clock()->Advance(test_advance);

  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// But when we navigate away very close to the original expiry of a prerender,
// we shouldn't expect it to be extended.
TEST_F(NoStatePrefetchTest, LinkManagerNavigateAwayNearExpiry) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  const base::TimeDelta time_to_live = base::Seconds(300);
  const base::TimeDelta abandon_time_to_live = base::Seconds(20);

  // We will expect the prerender to still be alive after advancing the clock
  // by first_advance. But, after second_advance, we expect it to have timed
  // out, demonstrating that you can't extend a prerender by navigating away
  // from its launcher.
  const base::TimeDelta first_advance = base::Seconds(298);
  const base::TimeDelta second_advance = base::Seconds(4);
  ASSERT_LT(first_advance, time_to_live);
  ASSERT_LT(time_to_live - first_advance, abandon_time_to_live);
  ASSERT_LT(time_to_live, first_advance + second_advance);

  no_state_prefetch_manager()->mutable_config().time_to_live = time_to_live;
  no_state_prefetch_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example2.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));

  tick_clock()->Advance(first_advance);
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));

  AbandonLastTrigger();
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));

  EXPECT_FALSE(no_state_prefetch_manager()->next_no_state_prefetch_contents());

  tick_clock()->Advance(second_advance);
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// When the user navigates away from a page, and then launches a new prerender,
// the new prerender should preempt the abandoned prerender even if the
// abandoned prerender hasn't expired.
TEST_F(NoStatePrefetchTest, LinkManagerNavigateAwayLaunchAnother) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  const base::TimeDelta time_to_live = base::Seconds(300);
  const base::TimeDelta abandon_time_to_live = base::Seconds(20);
  const base::TimeDelta test_advance = base::Seconds(5);
  ASSERT_LT(test_advance, time_to_live);
  ASSERT_GT(abandon_time_to_live, test_advance);

  no_state_prefetch_manager()->mutable_config().time_to_live = time_to_live;
  no_state_prefetch_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example.com");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  AbandonLastTrigger();

  tick_clock()->Advance(test_advance);

  GURL second_url("http://example2.com");
  FakeNoStatePrefetchContents* second_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          second_url, FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_TRUE(AddSimpleLinkTrigger(second_url));
  EXPECT_EQ(second_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(second_url));
}

// Prefetching the same URL twice during |time_to_live| results in a duplicate
// and is aborted.
TEST_F(NoStatePrefetchTest, NoStatePrefetchDuplicate) {
  const GURL kUrl("http://www.google.com/");
  const url::Origin kOrigin = url::Origin::Create(kUrl);
  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());

  // Prefetch the url once.
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      kUrl, kOrigin, ORIGIN_SAME_ORIGIN_SPECULATION, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(no_state_prefetch_manager()->AddSameOriginSpeculation(
      kUrl, nullptr, gfx::Size(), kOrigin));
  // Cancel the prefetch so that it is not reused.
  no_state_prefetch_manager()->CancelAllPrerenders();

  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      kUrl, kOrigin, ORIGIN_SAME_ORIGIN_SPECULATION,
      FINAL_STATUS_PROFILE_DESTROYED);

  // Prefetch again before time_to_live aborts, because it is a duplicate.
  tick_clock()->Advance(base::Seconds(1));
  EXPECT_FALSE(no_state_prefetch_manager()->AddSameOriginSpeculation(
      kUrl, nullptr, gfx::Size(), kOrigin));
  histogram_tester().ExpectBucketCount("Prerender.FinalStatus",
                                       FINAL_STATUS_DUPLICATE, 1);

  // Prefetch after time_to_live succeeds.
  tick_clock()->Advance(base::Minutes(net::HttpCache::kPrefetchReuseMins));
  EXPECT_TRUE(no_state_prefetch_manager()->AddSameOriginSpeculation(
      kUrl, nullptr, gfx::Size(), kOrigin));
}

// Make sure that if we prerender more requests than we support, that we launch
// them in the order given up until we reach MaxConcurrency, at which point we
// queue them and launch them in the order given. As well, insure that limits
// are enforced for the system as a whole and on a per launcher basis.
TEST_F(NoStatePrefetchTest, MaxConcurrencyTest) {
  struct TestConcurrency {
    size_t max_link_concurrency;
    size_t max_link_concurrency_per_launcher;
  };

  const TestConcurrency concurrencies_to_test[] = {
      {no_state_prefetch_manager()->config().max_link_concurrency,
       no_state_prefetch_manager()->config().max_link_concurrency_per_launcher},

      // With the system limit higher than the per launcher limit, the per
      // launcher limit should be in effect.
      {2, 1},

      // With the per launcher limit higher than system limit, the system limit
      // should be in effect.
      {2, 4},
  };

  size_t test_id = 0;
  for (const TestConcurrency& current_test : concurrencies_to_test) {
    test_id++;
    no_state_prefetch_manager()->mutable_config().max_link_concurrency =
        current_test.max_link_concurrency;
    no_state_prefetch_manager()
        ->mutable_config()
        .max_link_concurrency_per_launcher =
        current_test.max_link_concurrency_per_launcher;

    const size_t effective_max_link_concurrency =
        std::min(current_test.max_link_concurrency,
                 current_test.max_link_concurrency_per_launcher);

    std::vector<GURL> urls;
    std::vector<NoStatePrefetchContents*> no_state_prefetch_contentses;

    // Launch prerenders up to the maximum this launcher can support.
    for (size_t j = 0; j < effective_max_link_concurrency; ++j) {
      urls.push_back(GURL(base::StringPrintf(
          "http://google.com/use#%" PRIuS "%" PRIuS, j, test_id)));
      no_state_prefetch_contentses.push_back(
          no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
              urls.back(), FINAL_STATUS_USED));
      EXPECT_TRUE(AddSimpleLinkTrigger(urls.back()));
      EXPECT_FALSE(
          no_state_prefetch_manager()->next_no_state_prefetch_contents());
      EXPECT_TRUE(
          no_state_prefetch_contentses.back()->prefetching_has_started());
    }

    if (current_test.max_link_concurrency > effective_max_link_concurrency) {
      // We should be able to launch more prerenders on this system, but not for
      // the default launcher.
      GURL extra_url("http://google.com/extraurl");
      size_t trigger_count = CountExistingTriggers();
      EXPECT_FALSE(AddSimpleLinkTrigger(extra_url));
      EXPECT_EQ(trigger_count + 1, CountExistingTriggers());

      CancelLastTrigger();
      EXPECT_EQ(trigger_count, CountExistingTriggers());
    }

    GURL url_to_delay(
        base::StringPrintf("http://www.google.com/delayme#%" PRIuS, test_id));
    FakeNoStatePrefetchContents* no_state_prefetch_contents_to_delay =
        no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
            url_to_delay, FINAL_STATUS_USED);
    EXPECT_FALSE(AddSimpleLinkTrigger(url_to_delay));
    EXPECT_FALSE(
        no_state_prefetch_contents_to_delay->prefetching_has_started());
    EXPECT_TRUE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
    EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url_to_delay));
    for (size_t j = 0; j < effective_max_link_concurrency; ++j) {
      std::unique_ptr<NoStatePrefetchContents> entry =
          no_state_prefetch_manager()->FindAndUseEntry(urls[j]);
      EXPECT_EQ(no_state_prefetch_contentses[j], entry.get());
      EXPECT_TRUE(
          no_state_prefetch_contents_to_delay->prefetching_has_started());
    }

    std::unique_ptr<NoStatePrefetchContents> entry =
        no_state_prefetch_manager()->FindAndUseEntry(url_to_delay);
    EXPECT_EQ(no_state_prefetch_contents_to_delay, entry.get());
    EXPECT_FALSE(
        no_state_prefetch_manager()->next_no_state_prefetch_contents());
  }
}

// Flaky on Android: https://crbug.com/1105908
TEST_F(NoStatePrefetchTest, DISABLED_AliasURLTest) {
  SetConcurrency(7);

  GURL url("http://www.google.com/");
  GURL alias_url1("http://www.google.com/index.html");
  GURL alias_url2("http://google.com/");
  GURL not_an_alias_url("http://google.com/index.html");
  std::vector<GURL> alias_urls;
  alias_urls.push_back(alias_url1);
  alias_urls.push_back(alias_url2);

  // Test that all of the aliases work, but not_an_alias_url does not.
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(not_an_alias_url));
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(alias_url1);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
  no_state_prefetch_manager()->ClearPrefetchInformationForTesting();

  no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  entry = no_state_prefetch_manager()->FindAndUseEntry(alias_url2);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
  no_state_prefetch_manager()->ClearPrefetchInformationForTesting();

  no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  entry = no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
  no_state_prefetch_manager()->ClearPrefetchInformationForTesting();

  // Test that alias URLs can be added.
  no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(AddSimpleLinkTrigger(alias_url1));
  EXPECT_TRUE(AddSimpleLinkTrigger(alias_url2));
  entry = no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Tests that prerendering is cancelled when the source render view does not
// exist.  On failure, the DCHECK in CreateNoStatePrefetchContents() above
// should be triggered.
TEST_F(NoStatePrefetchTest, SourceRenderViewClosed) {
  GURL url("http://www.google.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);
  AddLinkTrigger(url, url, 100, 200, 300);
  EXPECT_FALSE(LastTriggerExists());
}

// Tests that prerendering is cancelled when we launch a second prerender of
// the same target within a short time interval.
TEST_F(NoStatePrefetchTest, RecentlyVisited) {
  GURL url("http://www.google.com/");

  no_state_prefetch_manager()->RecordNavigation(url);

  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_RECENTLY_VISITED);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_started());
}

TEST_F(NoStatePrefetchTest, NotSoRecentlyVisited) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  GURL url("http://www.google.com/");

  no_state_prefetch_manager()->RecordNavigation(url);
  tick_clock()->Advance(base::Milliseconds(
      UnitTestNoStatePrefetchManager::kNavigationRecordWindowMs + 500));

  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Tests that the prerender manager matches include the fragment.
TEST_F(NoStatePrefetchTest, FragmentMatchesTest) {
  GURL fragment_url("http://www.google.com/#test");

  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          fragment_url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(fragment_url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(fragment_url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Tests that the prerender manager uses fragment references when matching
// prerender URLs in the case a different fragment is in both URLs.
TEST_F(NoStatePrefetchTest, FragmentsDifferTest) {
  GURL fragment_url("http://www.google.com/#test");
  GURL other_fragment_url("http://www.google.com/#other_test");

  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          fragment_url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(fragment_url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());

  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(other_fragment_url));

  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(fragment_url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Make sure that clearing works as expected.
TEST_F(NoStatePrefetchTest, ClearTest) {
  GURL url("http://www.google.com/");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  no_state_prefetch_manager()->ClearData(
      NoStatePrefetchManager::CLEAR_PRERENDER_CONTENTS);
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Make sure canceling works as expected.
TEST_F(NoStatePrefetchTest, CancelAllTest) {
  GURL url("http://www.google.com/");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  no_state_prefetch_manager()->CancelAllPrerenders();
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

class NoStatePrefetchFallbackToPreconnectTest
    : public base::test::WithFeatureOverride,
      public NoStatePrefetchTest {
 public:
  NoStatePrefetchFallbackToPreconnectTest()
      : base::test::WithFeatureOverride(
            features::kPrerenderFallbackToPreconnect) {}
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    NoStatePrefetchFallbackToPreconnectTest);

// Test that when prefetch fails and ...
// - the kPrerenderFallbackToPreconnect experiment is not enabled, a prefetch
//   initiated by <link rel=prerender> does not result in a preconnect.
// - the kPrerenderFallbackToPreconnect experiment is enabled, a prefetch
//   initiated by <link rel=prerender> actually results in preconnect.
TEST_P(NoStatePrefetchFallbackToPreconnectTest, LinkRelPrerender) {
  const GURL kURL(GURL("http://www.example.com"));
  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  // Prefetch should be disabled on low memory devices.
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(AddSimpleLinkTrigger(kURL));

  if (base::FeatureList::IsEnabled(features::kPrerenderFallbackToPreconnect)) {
    // Verify that the prefetch request falls back to a preconnect request.
    EXPECT_EQ(1u, loading_predictor->GetActiveHintsSizeForTesting());
    auto& active_hints = loading_predictor->active_hints_for_testing();
    auto it = active_hints.find(kURL);
    EXPECT_NE(it, active_hints.end());
  } else {
    EXPECT_EQ(0u, loading_predictor->GetActiveHintsSizeForTesting());
  }
}

TEST_F(NoStatePrefetchTest, LinkRelStillAllowedWhenDisabled) {
  DisablePrerender();
  GURL url("http://www.google.com/");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, url::Origin::Create(GURL("https://www.notgoogle.com")),
          ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(NoStatePrefetchTest, LinkRelAllowedOnCellular) {
  EnablePrerender();
  GURL url("http://www.example.com");
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4GMetered);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_COST_METERED,
            net::NetworkChangeNotifier::GetConnectionCost());
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, url::Origin::Create(GURL("https://www.notexample.com")),
          ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(NoStatePrefetchTest, LinkManagerCancel) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());
  CancelLastTrigger();

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

TEST_F(NoStatePrefetchTest, LinkManagerAbandon) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());
  AbandonLastTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(NoStatePrefetchTest, LinkManagerAbandonThenCancel) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());
  AbandonLastTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));

  CancelLastTrigger();
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Flaky on Android, crbug.com/1087876.
// Flaky on Mac and Linux, crbug.com/1087735.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#define MAYBE_LinkManagerAddTwiceCancelTwice \
  DISABLED_LinkManagerAddTwiceCancelTwice
#else
#define MAYBE_LinkManagerAddTwiceCancelTwice LinkManagerAddTwiceCancelTwice
#endif
TEST_F(NoStatePrefetchTest, MAYBE_LinkManagerAddTwiceCancelTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  CancelFirstTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  CancelFirstTrigger();

  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// TODO(gavinp): Update this test after abandon has an effect on Prerenders,
// like shortening the timeouts.
// Flaky on Android and Linux, crbug.com/1087876 & crbug.com/1087736.
TEST_F(NoStatePrefetchTest, DISABLED_LinkManagerAddTwiceAbandonTwiceUseTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  AbandonFirstTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  AbandonFirstTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

// TODO(gavinp): After abandon shortens the expire time on a Prerender,
// add a series of tests testing advancing the time by either the abandon
// or normal expire, and verifying the expected behaviour with groups
// of links.
TEST_F(NoStatePrefetchTest, LinkManagerExpireThenCancel) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  tick_clock()->Advance(no_state_prefetch_manager()->config().time_to_live +
                        base::Seconds(1));

  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());

  // FindEntry will have a side-effect of pruning expired prerenders.
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));

  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

TEST_F(NoStatePrefetchTest, LinkManagerExpireThenAddAgain) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* first_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(first_no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(
      first_no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  tick_clock()->Advance(no_state_prefetch_manager()->config().time_to_live +
                        base::Seconds(1));

  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
  FakeNoStatePrefetchContents* second_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(second_no_state_prefetch_contents->prefetching_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(second_no_state_prefetch_contents, entry.get());
}

// Flaky on Android, crbug.com/1087876.
TEST_F(NoStatePrefetchTest, DISABLED_LinkManagerCancelThenAddAgain) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  FakeNoStatePrefetchContents* first_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(first_no_state_prefetch_contents->prefetching_has_started());
  EXPECT_FALSE(
      first_no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  CancelLastTrigger();
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_TRUE(
      first_no_state_prefetch_contents->prefetching_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));

  // A cancelled NoStatePrefetch is counted as a prefetch recently happened. A
  // new attempt to prefetch should return as duplicate.
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Creates two prerenders, one of which should be blocked by the
// max_link_concurrency; abandons both of them and waits to make sure both
// are cleared from the NoStatePrefetchLinkManager.
TEST_F(NoStatePrefetchTest, DISABLED_LinkManagerAbandonInactivePrerender) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(no_state_prefetch_manager()->config().abandon_time_to_live,
            no_state_prefetch_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          first_url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(first_url));

  GURL second_url("http://www.neverlaunched.com");
  EXPECT_FALSE(AddSimpleLinkTrigger(second_url));

  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());

  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));

  AbandonFirstTrigger();
  AbandonFirstTrigger();

  tick_clock()->Advance(
      no_state_prefetch_manager()->config().abandon_time_to_live +
      base::Seconds(1));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

// Creates two prerenders, one of which should be blocked by the
// max_link_concurrency; uses one after the max wait to launch, and
// ensures the second prerender does not start.
TEST_F(NoStatePrefetchTest, LinkManagerWaitToLaunchNotLaunched) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(no_state_prefetch_manager()->config().max_wait_to_launch,
            no_state_prefetch_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  FakeNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          first_url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(first_url));

  GURL second_url("http://www.neverlaunched.com");
  EXPECT_FALSE(AddSimpleLinkTrigger(second_url));

  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());

  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));

  tick_clock()->Advance(
      no_state_prefetch_manager()->config().max_wait_to_launch +
      base::Seconds(1));
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));

  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(first_url);
  EXPECT_EQ(no_state_prefetch_contents, entry.get());

  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

// Creates two prerenders, one of which should start when the first one expires.
TEST_F(NoStatePrefetchTest, LinkManagerExpireRevealingLaunch) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(no_state_prefetch_manager()->config().max_wait_to_launch,
            no_state_prefetch_manager()->config().time_to_live);

  GURL first_url("http://www.willexpire.com");
  FakeNoStatePrefetchContents* first_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          first_url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(first_url));
  EXPECT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(first_url));

  // Insert the second prerender so it will be still be launchable when the
  // first expires.
  const base::TimeDelta wait_to_launch_second_prerender =
      no_state_prefetch_manager()->config().time_to_live -
      no_state_prefetch_manager()->config().max_wait_to_launch +
      base::Seconds(2);
  const base::TimeDelta wait_for_first_prerender_to_expire =
      no_state_prefetch_manager()->config().time_to_live -
      wait_to_launch_second_prerender + base::Seconds(1);
  ASSERT_LT(
      no_state_prefetch_manager()->config().time_to_live,
      wait_to_launch_second_prerender + wait_for_first_prerender_to_expire);
  ASSERT_GT(
      no_state_prefetch_manager()->config().max_wait_to_launch.InSeconds(),
      wait_for_first_prerender_to_expire.InSeconds());

  tick_clock()->Advance(wait_to_launch_second_prerender);
  GURL second_url("http://www.willlaunch.com");
  FakeNoStatePrefetchContents* second_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          second_url, FINAL_STATUS_USED);
  EXPECT_FALSE(AddSimpleLinkTrigger(second_url));

  // The first prerender is still running, but the second has not yet launched.
  EXPECT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));

  // The first prerender should have died, giving life to the second one.
  tick_clock()->Advance(wait_for_first_prerender_to_expire);
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(first_url));
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(second_url);
  EXPECT_EQ(second_no_state_prefetch_contents, entry.get());
}

TEST_F(NoStatePrefetchTest, NoStatePrefetchContentsIsValidHttpMethod) {
  EXPECT_TRUE(IsValidHttpMethod("GET"));
  EXPECT_TRUE(IsValidHttpMethod("HEAD"));
  EXPECT_FALSE(IsValidHttpMethod("OPTIONS"));
  EXPECT_FALSE(IsValidHttpMethod("POST"));
  EXPECT_FALSE(IsValidHttpMethod("TRACE"));
  EXPECT_FALSE(IsValidHttpMethod("WHATEVER"));
}

TEST_F(NoStatePrefetchTest, NoPrerenderInSingleProcess) {
  GURL url("http://www.google.com/");
  auto* command_line = base::CommandLine::ForCurrentProcess();
  ASSERT_TRUE(command_line != nullptr);
  command_line->AppendSwitch(switches::kSingleProcess);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_SINGLE_PROCESS, 1);
}

}  // namespace prerender
