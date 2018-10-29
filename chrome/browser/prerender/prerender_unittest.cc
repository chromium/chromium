// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <utility>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_link_manager.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_origin.h"
#include "chrome/browser/prerender/prerender_test_utils.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/prerender_types.h"
#include "chrome/common/prerender_util.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/network_change_notifier.h"
#include "net/http/http_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;
using content::Referrer;

namespace prerender {

class UnitTestPrerenderManager;

namespace {

class DummyPrerenderContents : public PrerenderContents {
 public:
  DummyPrerenderContents(UnitTestPrerenderManager* test_prerender_manager,
                         const GURL& url,
                         Origin origin,
                         FinalStatus expected_final_status);

  ~DummyPrerenderContents() override;

  void StartPrerendering(
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace) override;

  bool GetChildId(int* child_id) const override {
    // Having a default child_id of -1 forces pending prerenders not to fail
    // on session storage and cross domain checking.
    *child_id = -1;
    return true;
  }

  bool GetRouteId(int* route_id) const override {
    *route_id = route_id_;
    return true;
  }

  FinalStatus expected_final_status() const { return expected_final_status_; }

  bool prerendering_has_been_cancelled() const {
    return PrerenderContents::prerendering_has_been_cancelled();
  }

 private:
  static int g_next_route_id_;
  int route_id_;

  UnitTestPrerenderManager* test_prerender_manager_;
  FinalStatus expected_final_status_;
};

class TestNetworkBytesChangedObserver
    : public prerender::PrerenderHandle::Observer {
 public:
  TestNetworkBytesChangedObserver() : network_bytes_changed_(false) {}

  // prerender::PrerenderHandle::Observer
  void OnPrerenderStart(PrerenderHandle* prerender_handle) override {}
  void OnPrerenderStopLoading(PrerenderHandle* prerender_handle) override {}
  void OnPrerenderDomContentLoaded(PrerenderHandle* prerender_handle) override {
  }
  void OnPrerenderStop(PrerenderHandle* prerender_handle) override {}
  void OnPrerenderNetworkBytesChanged(
      PrerenderHandle* prerender_handle) override {
    network_bytes_changed_ = true;
  }

  bool network_bytes_changed() const { return network_bytes_changed_; }

 private:
  bool network_bytes_changed_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkBytesChangedObserver);
};

int DummyPrerenderContents::g_next_route_id_ = 0;

const gfx::Size kSize(640, 480);

const uint32_t kDefaultRelTypes = PrerenderRelTypePrerender;

}  // namespace

class UnitTestPrerenderManager : public PrerenderManager {
 public:
  using PrerenderManager::kMinTimeBetweenPrerendersMs;
  using PrerenderManager::kNavigationRecordWindowMs;

  explicit UnitTestPrerenderManager(Profile* profile)
      : PrerenderManager(profile) {
    set_rate_limit_enabled(false);
  }

  ~UnitTestPrerenderManager() override {}

  // From KeyedService, via PrererenderManager:
  void Shutdown() override {
    if (next_prerender_contents())
      next_prerender_contents_->Destroy(FINAL_STATUS_MANAGER_SHUTDOWN);
    PrerenderManager::Shutdown();
  }

  // From PrerenderManager:
  void MoveEntryToPendingDelete(PrerenderContents* entry,
                                FinalStatus final_status) override {
    if (entry == next_prerender_contents_.get())
      return;
    PrerenderManager::MoveEntryToPendingDelete(entry, final_status);
  }

  PrerenderContents* FindEntry(const GURL& url) {
    DeleteOldEntries();
    to_delete_prerenders_.clear();
    PrerenderData* data = FindPrerenderData(url, nullptr);
    return data ? data->contents() : nullptr;
  }

  std::unique_ptr<PrerenderContents> FindAndUseEntry(const GURL& url) {
    PrerenderData* prerender_data = FindPrerenderData(url, nullptr);
    if (!prerender_data)
      return nullptr;
    auto to_erase =
        FindIteratorForPrerenderContents(prerender_data->contents());
    CHECK(to_erase != active_prerenders_.end());
    std::unique_ptr<PrerenderContents> prerender_contents =
        prerender_data->ReleaseContents();
    active_prerenders_.erase(to_erase);

    prerender_contents->PrepareForUse();
    return prerender_contents;
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      FinalStatus expected_final_status) {
    return SetNextPrerenderContents(std::make_unique<DummyPrerenderContents>(
        this, url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN,
        expected_final_status));
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      Origin origin,
      FinalStatus expected_final_status) {
    return SetNextPrerenderContents(std::make_unique<DummyPrerenderContents>(
        this, url, origin, expected_final_status));
  }

  DummyPrerenderContents* CreateNextPrerenderContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      FinalStatus expected_final_status) {
    auto prerender_contents = std::make_unique<DummyPrerenderContents>(
        this, url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN,
        expected_final_status);
    for (const GURL& alias : alias_urls)
      EXPECT_TRUE(prerender_contents->AddAliasURL(alias));
    return SetNextPrerenderContents(std::move(prerender_contents));
  }

  void set_rate_limit_enabled(bool enabled) {
    mutable_config().rate_limit_enabled = enabled;
  }

  PrerenderContents* next_prerender_contents() {
    return next_prerender_contents_.get();
  }

  PrerenderContents* GetPrerenderContentsForRoute(int child_id,
                                                  int route_id) const override {
    // Overridden for the PrerenderLinkManager's pending prerender logic.
    auto it = prerender_contents_map_.find(std::make_pair(child_id, route_id));
    return it != prerender_contents_map_.end() ? it->second : nullptr;
  }

  void DummyPrerenderContentsStarted(int child_id,
                                     int route_id,
                                     PrerenderContents* prerender_contents) {
    prerender_contents_map_[std::make_pair(child_id, route_id)] =
        prerender_contents;
  }

  void DummyPrerenderContentsDestroyed(int child_id,
                                       int route_id) {
    prerender_contents_map_.erase(std::make_pair(child_id, route_id));
  }

  void SetIsLowEndDevice(bool is_low_end_device) {
    is_low_end_device_ = is_low_end_device;
  }

 private:
  bool IsLowEndDevice() const override { return is_low_end_device_; }

  DummyPrerenderContents* SetNextPrerenderContents(
      std::unique_ptr<DummyPrerenderContents> prerender_contents) {
    CHECK(!next_prerender_contents_);
    DummyPrerenderContents* contents_ptr = prerender_contents.get();
    next_prerender_contents_ = std::move(prerender_contents);
    return contents_ptr;
  }

  std::unique_ptr<PrerenderContents> CreatePrerenderContents(
      const GURL& url,
      const Referrer& referrer,
      Origin origin) override {
    CHECK(next_prerender_contents_);
    EXPECT_EQ(url, next_prerender_contents_->prerender_url());
    EXPECT_EQ(origin, next_prerender_contents_->origin());
    return std::move(next_prerender_contents_);
  }

  // Maintain a map from route pairs to PrerenderContents for
  // GetPrerenderContentsForRoute.
  using PrerenderContentsMap =
      std::map<std::pair<int, int>, PrerenderContents*>;
  PrerenderContentsMap prerender_contents_map_;

  std::unique_ptr<PrerenderContents> next_prerender_contents_;
  bool is_low_end_device_;
};

class MockNetworkChangeNotifier4G : public net::NetworkChangeNotifier {
 public:
  ConnectionType GetCurrentConnectionType() const override {
    return NetworkChangeNotifier::CONNECTION_4G;
  }
};

DummyPrerenderContents::DummyPrerenderContents(
    UnitTestPrerenderManager* test_prerender_manager,
    const GURL& url,
    Origin origin,
    FinalStatus expected_final_status)
    : PrerenderContents(test_prerender_manager,
                        nullptr,
                        url,
                        Referrer(),
                        origin),
      route_id_(g_next_route_id_++),
      test_prerender_manager_(test_prerender_manager),
      expected_final_status_(expected_final_status) {}

DummyPrerenderContents::~DummyPrerenderContents() {
  EXPECT_EQ(expected_final_status_, final_status());
  test_prerender_manager_->DummyPrerenderContentsDestroyed(-1, route_id_);
}

void DummyPrerenderContents::StartPrerendering(
    const gfx::Rect& bounds,
    content::SessionStorageNamespace* session_storage_namespace) {
  load_start_time_ = test_prerender_manager_->GetCurrentTimeTicks();
  prerendering_has_started_ = true;
  test_prerender_manager_->DummyPrerenderContentsStarted(-1, route_id_, this);
  NotifyPrerenderStart();
}

class PrerenderTest : public testing::Test {
 public:
  static const int kDefaultChildId = -1;
  static const int kDefaultRenderViewRouteId = -1;

  PrerenderTest()
      : prerender_manager_(new UnitTestPrerenderManager(&profile_)),
        prerender_link_manager_(
            new PrerenderLinkManager(prerender_manager_.get())),
        last_prerender_id_(0),
        field_trial_list_(nullptr) {
    prerender::PrerenderManager::SetMode(
        prerender::PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);
    prerender_manager()->SetIsLowEndDevice(false);
  }

  ~PrerenderTest() override {
    prerender_link_manager_->OnChannelClosing(kDefaultChildId);
    prerender_link_manager_->Shutdown();
    prerender_manager_->Shutdown();
  }

  void TearDown() override {
    base::FieldTrialParamAssociator::GetInstance()->ClearAllParamsForTesting();
  }

  base::SimpleTestTickClock* tick_clock() { return &tick_clock_; }

  UnitTestPrerenderManager* prerender_manager() {
    return prerender_manager_.get();
  }

  PrerenderLinkManager* prerender_link_manager() {
    return prerender_link_manager_.get();
  }

  Profile* profile() { return &profile_; }

  void SetConcurrency(size_t concurrency) {
    prerender_manager()->mutable_config().max_link_concurrency_per_launcher =
        concurrency;
    prerender_manager()->mutable_config().max_link_concurrency =
        std::max(prerender_manager()->mutable_config().max_link_concurrency,
                 concurrency);
  }

  bool IsEmptyPrerenderLinkManager() const {
    return prerender_link_manager_->IsEmpty();
  }

  int last_prerender_id() const {
    return last_prerender_id_;
  }

  int GetNextPrerenderID() {
    return ++last_prerender_id_;
  }

  bool LauncherHasRunningPrerender(int child_id, int prerender_id) {
    PrerenderLinkManager::LinkPrerender* prerender =
        prerender_link_manager()->FindByLauncherChildIdAndPrerenderId(
            child_id, prerender_id);
    return prerender && prerender->handle;
  }

  bool LauncherHasScheduledPrerender(int child_id, int prerender_id) {
    PrerenderLinkManager::LinkPrerender* prerender =
        prerender_link_manager()->FindByLauncherChildIdAndPrerenderId(
            child_id, prerender_id);
    return prerender != nullptr;
  }

  // Shorthand to add a simple prerender with a reasonable source. Returns
  // true iff the prerender has been added to the PrerenderManager by the
  // PrerenderLinkManager and the PrerenderManager returned a handle.
  bool AddSimplePrerender(const GURL& url) {
    prerender_link_manager()->OnAddPrerender(
        kDefaultChildId, GetNextPrerenderID(), url, kDefaultRelTypes,
        content::Referrer(), kSize, kDefaultRenderViewRouteId);
    return LauncherHasRunningPrerender(kDefaultChildId, last_prerender_id());
  }

  void DisablePrerender() {
    profile_.GetPrefs()->SetInteger(
        prefs::kNetworkPredictionOptions,
        chrome_browser_net::NETWORK_PREDICTION_NEVER);
  }

  void EnablePrerender() {
    profile_.GetPrefs()->SetInteger(
        prefs::kNetworkPredictionOptions,
        chrome_browser_net::NETWORK_PREDICTION_ALWAYS);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  // Needed to pass PrerenderManager's DCHECKs.
  content::TestBrowserThreadBundle test_browser_thread_bundle_;

  TestingProfile profile_;
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<UnitTestPrerenderManager> prerender_manager_;
  std::unique_ptr<PrerenderLinkManager> prerender_link_manager_;
  int last_prerender_id_;
  base::HistogramTester histogram_tester_;

  // An instance of base::FieldTrialList is necessary in order to initialize
  // global state.
  base::FieldTrialList field_trial_list_;

  // Restore prerender mode after this test finishes running.
  test_utils::RestorePrerenderMode restore_prerender_mode_;
};

TEST_F(PrerenderTest, RespectsThirdPartyCookiesPref) {
  GURL url("http://www.google.com/");
  ASSERT_TRUE(IsNoStatePrefetchEnabled());

  profile()->GetPrefs()->SetBoolean(prefs::kBlockThirdPartyCookies, true);
  EXPECT_FALSE(AddSimplePrerender(url));
  histogram_tester().ExpectUniqueSample(
      "Prerender.FinalStatus", FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES, 1);
}

TEST_F(PrerenderTest, NoStatePrefetchMode) {
  GURL url("http://www.google.com/");
  test_utils::RestorePrerenderMode restore_prerender_mode;

  prerender_manager()->SetMode(
      PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(PREFETCH_ONLY, prerender_contents->prerender_mode());
}

TEST_F(PrerenderTest, SimpleLoadMode) {
  GURL url("http://www.google.com/");
  test_utils::RestorePrerenderMode restore_prerender_mode;

  prerender_manager()->SetMode(
      PrerenderManager::PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT);
  EXPECT_FALSE(AddSimplePrerender(url));
}

TEST_F(PrerenderTest, PrerenderDisabledOnLowEndDevice) {
  GURL url("http://www.google.com/");
  ASSERT_TRUE(IsNoStatePrefetchEnabled());
  prerender_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(AddSimplePrerender(url));
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_LOW_END_DEVICE, 1);
}

TEST_F(PrerenderTest, FoundTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

// Make sure that if queue a request, and a second prerender request for the
// same URL comes in, that the second request attaches to the first prerender,
// and we don't use the second prerender contents.
// This test is the same as the "DuplicateTest" above, but for NoStatePrefetch.
TEST_F(PrerenderTest, DuplicateTest_NoStatePrefetch) {
  test_utils::RestorePrerenderMode restore_prerender_mode;
  prerender_manager()->SetMode(
      PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);

  SetConcurrency(2);
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_FALSE(prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->prerendering_has_started());

  DummyPrerenderContents* prerender_contents1 =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_EQ(prerender_contents1,
            prerender_manager()->next_prerender_contents());
  EXPECT_FALSE(prerender_contents1->prerendering_has_started());

  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

// Ensure that we expire a prerendered page after the max. permitted time.
TEST_F(PrerenderTest, ExpireTest) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_FALSE(prerender_manager()->next_prerender_contents());
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  tick_clock()->Advance(prerender_manager()->config().time_to_live +
                        TimeDelta::FromSeconds(1));
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
}

// Ensure that we don't launch prerenders of bad urls (in this case, a mailto:
// url)
TEST_F(PrerenderTest, BadURLTest) {
  GURL url("mailto:test@gmail.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_UNSUPPORTED_SCHEME);
  EXPECT_FALSE(AddSimplePrerender(url));
  EXPECT_FALSE(prerender_contents->prerendering_has_started());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_FALSE(prerender_manager()->FindEntry(url));
}

// When the user navigates away from a page, the prerenders it launched should
// have their time to expiry shortened from the default time to live.
TEST_F(PrerenderTest, LinkManagerNavigateAwayExpire) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  const TimeDelta time_to_live = TimeDelta::FromSeconds(300);
  const TimeDelta abandon_time_to_live = TimeDelta::FromSeconds(20);
  const TimeDelta test_advance = TimeDelta::FromSeconds(22);
  ASSERT_LT(test_advance, time_to_live);
  ASSERT_LT(abandon_time_to_live, test_advance);

  prerender_manager()->mutable_config().time_to_live = time_to_live;
  prerender_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(url,
                                                       FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(prerender_manager()->next_prerender_contents());
  tick_clock()->Advance(test_advance);

  EXPECT_FALSE(prerender_manager()->FindEntry(url));
}

// But when we navigate away very close to the original expiry of a prerender,
// we shouldn't expect it to be extended.
TEST_F(PrerenderTest, LinkManagerNavigateAwayNearExpiry) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  const TimeDelta time_to_live = TimeDelta::FromSeconds(300);
  const TimeDelta abandon_time_to_live = TimeDelta::FromSeconds(20);

  // We will expect the prerender to still be alive after advancing the clock
  // by first_advance. But, after second_advance, we expect it to have timed
  // out, demonstrating that you can't extend a prerender by navigating away
  // from its launcher.
  const TimeDelta first_advance = TimeDelta::FromSeconds(298);
  const TimeDelta second_advance = TimeDelta::FromSeconds(4);
  ASSERT_LT(first_advance, time_to_live);
  ASSERT_LT(time_to_live - first_advance, abandon_time_to_live);
  ASSERT_LT(time_to_live, first_advance + second_advance);

  prerender_manager()->mutable_config().time_to_live = time_to_live;
  prerender_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example2.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(url,
                                                       FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));

  tick_clock()->Advance(first_advance);
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));

  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));

  EXPECT_FALSE(prerender_manager()->next_prerender_contents());

  tick_clock()->Advance(second_advance);
  EXPECT_FALSE(prerender_manager()->FindEntry(url));
}

// When the user navigates away from a page, and then launches a new prerender,
// the new prerender should preempt the abandoned prerender even if the
// abandoned prerender hasn't expired.
TEST_F(PrerenderTest, LinkManagerNavigateAwayLaunchAnother) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  const TimeDelta time_to_live = TimeDelta::FromSeconds(300);
  const TimeDelta abandon_time_to_live = TimeDelta::FromSeconds(20);
  const TimeDelta test_advance = TimeDelta::FromSeconds(5);
  ASSERT_LT(test_advance, time_to_live);
  ASSERT_GT(abandon_time_to_live, test_advance);

  prerender_manager()->mutable_config().time_to_live = time_to_live;
  prerender_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example.com");
  prerender_manager()->CreateNextPrerenderContents(url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimplePrerender(url));
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());

  tick_clock()->Advance(test_advance);

  GURL second_url("http://example2.com");
  DummyPrerenderContents* second_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          second_url, FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_TRUE(AddSimplePrerender(second_url));
  EXPECT_EQ(second_prerender_contents,
            prerender_manager()->FindEntry(second_url));
}

// Prefetching the same URL twice during |time_to_live| results in a duplicate
// and is aborted.
TEST_F(PrerenderTest, NoStatePrefetchDuplicate) {
  const GURL kUrl("http://www.google.com/");

  test_utils::RestorePrerenderMode restore_prerender_mode;
  prerender_manager()->SetMode(
      PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH);
  prerender_manager()->SetTickClockForTesting(tick_clock());

  // Prefetch the url once.
  prerender_manager()->CreateNextPrerenderContents(kUrl, ORIGIN_OMNIBOX,
                                                   FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(
      prerender_manager()->AddPrerenderFromOmnibox(kUrl, nullptr, gfx::Size()));
  // Cancel the prerender so that it is not reused.
  prerender_manager()->CancelAllPrerenders();

  prerender_manager()->CreateNextPrerenderContents(
      kUrl, ORIGIN_OMNIBOX, FINAL_STATUS_MANAGER_SHUTDOWN);

  // Prefetching again before time_to_live aborts, because it is a duplicate.
  tick_clock()->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(
      prerender_manager()->AddPrerenderFromOmnibox(kUrl, nullptr, gfx::Size()));
  histogram_tester().ExpectBucketCount("Prerender.FinalStatus",
                                       FINAL_STATUS_DUPLICATE, 1);

  // Prefetching after time_to_live succeeds.
  tick_clock()->Advance(
      base::TimeDelta::FromMinutes(net::HttpCache::kPrefetchReuseMins));
  EXPECT_TRUE(
      prerender_manager()->AddPrerenderFromOmnibox(kUrl, nullptr, gfx::Size()));
}

// Make sure that if we prerender more requests than we support, that we launch
// them in the order given up until we reach MaxConcurrency, at which point we
// queue them and launch them in the order given. As well, insure that limits
// are enforced for the system as a whole and on a per launcher basis.
TEST_F(PrerenderTest, MaxConcurrencyTest) {
  struct TestConcurrency {
    size_t max_link_concurrency;
    size_t max_link_concurrency_per_launcher;
  };

  const TestConcurrency concurrencies_to_test[] = {
      {prerender_manager()->config().max_link_concurrency,
       prerender_manager()->config().max_link_concurrency_per_launcher},

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
    prerender_manager()->mutable_config().max_link_concurrency =
        current_test.max_link_concurrency;
    prerender_manager()->mutable_config().max_link_concurrency_per_launcher =
        current_test.max_link_concurrency_per_launcher;

    const size_t effective_max_link_concurrency =
        std::min(current_test.max_link_concurrency,
                 current_test.max_link_concurrency_per_launcher);

    std::vector<GURL> urls;
    std::vector<PrerenderContents*> prerender_contentses;

    // Launch prerenders up to the maximum this launcher can support.
    for (size_t j = 0; j < effective_max_link_concurrency; ++j) {
      urls.push_back(GURL(base::StringPrintf(
          "http://google.com/use#%" PRIuS "%" PRIuS, j, test_id)));
      prerender_contentses.push_back(
          prerender_manager()->CreateNextPrerenderContents(urls.back(),
                                                           FINAL_STATUS_USED));
      EXPECT_TRUE(AddSimplePrerender(urls.back()));
      EXPECT_FALSE(prerender_manager()->next_prerender_contents());
      EXPECT_TRUE(prerender_contentses.back()->prerendering_has_started());
    }

    if (current_test.max_link_concurrency > effective_max_link_concurrency) {
      // We should be able to launch more prerenders on this system, but not for
      // the default launcher.
      GURL extra_url("http://google.com/extraurl");
      EXPECT_FALSE(AddSimplePrerender(extra_url));
      const int prerender_id = last_prerender_id();
      EXPECT_TRUE(LauncherHasScheduledPrerender(kDefaultChildId,
                                                prerender_id));
      prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                                  prerender_id);
      EXPECT_FALSE(LauncherHasScheduledPrerender(kDefaultChildId,
                                                 prerender_id));
    }

    GURL url_to_delay(
        base::StringPrintf("http://www.google.com/delayme#%" PRIuS, test_id));
    DummyPrerenderContents* prerender_contents_to_delay =
        prerender_manager()->CreateNextPrerenderContents(url_to_delay,
                                                         FINAL_STATUS_USED);
    EXPECT_FALSE(AddSimplePrerender(url_to_delay));
    EXPECT_FALSE(prerender_contents_to_delay->prerendering_has_started());
    EXPECT_TRUE(prerender_manager()->next_prerender_contents());
    EXPECT_FALSE(prerender_manager()->FindEntry(url_to_delay));
    for (size_t j = 0; j < effective_max_link_concurrency; ++j) {
      std::unique_ptr<PrerenderContents> entry =
          prerender_manager()->FindAndUseEntry(urls[j]);
      EXPECT_EQ(prerender_contentses[j], entry.get());
      EXPECT_TRUE(prerender_contents_to_delay->prerendering_has_started());
    }

    std::unique_ptr<PrerenderContents> entry =
        prerender_manager()->FindAndUseEntry(url_to_delay);
    EXPECT_EQ(prerender_contents_to_delay, entry.get());
    EXPECT_FALSE(prerender_manager()->next_prerender_contents());
  }
}

TEST_F(PrerenderTest, AliasURLTest) {
  ASSERT_TRUE(IsNoStatePrefetchEnabled());
  SetConcurrency(7);

  GURL url("http://www.google.com/");
  GURL alias_url1("http://www.google.com/index.html");
  GURL alias_url2("http://google.com/");
  GURL not_an_alias_url("http://google.com/index.html");
  std::vector<GURL> alias_urls;
  alias_urls.push_back(alias_url1);
  alias_urls.push_back(alias_url2);

  // Test that all of the aliases work, but not_an_alias_url does not.
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  ASSERT_FALSE(prerender_manager()->FindEntry(not_an_alias_url));
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(alias_url1);
  ASSERT_EQ(prerender_contents, entry.get());
  prerender_manager()->ClearPrefetchInformationForTesting();

  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  entry = prerender_manager()->FindAndUseEntry(alias_url2);
  ASSERT_EQ(prerender_contents, entry.get());
  prerender_manager()->ClearPrefetchInformationForTesting();

  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  entry = prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
  prerender_manager()->ClearPrefetchInformationForTesting();

  // Test that alias URLs can be added.
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
          url, alias_urls, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(AddSimplePrerender(alias_url1));
  EXPECT_TRUE(AddSimplePrerender(alias_url2));
  entry = prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

TEST_F(PrerenderTest, PendingPrerenderTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));

  int child_id;
  int route_id;
  ASSERT_TRUE(prerender_contents->GetChildId(&child_id));
  ASSERT_TRUE(prerender_contents->GetRouteId(&route_id));

  GURL pending_url("http://news.google.com/");

  // Schedule a pending prerender launched from the prerender.
  DummyPrerenderContents* pending_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          pending_url,
          ORIGIN_GWS_PRERENDER,
          FINAL_STATUS_USED);
  prerender_link_manager()->OnAddPrerender(
      child_id, GetNextPrerenderID(), pending_url, kDefaultRelTypes,
      Referrer(url, network::mojom::ReferrerPolicy::kDefault), kSize, route_id);
  EXPECT_FALSE(LauncherHasRunningPrerender(child_id, last_prerender_id()));
  EXPECT_FALSE(pending_prerender_contents->prerendering_has_started());

  // Use the referring prerender.
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());

  // The pending prerender should start now.
  EXPECT_TRUE(LauncherHasRunningPrerender(child_id, last_prerender_id()));
  EXPECT_TRUE(pending_prerender_contents->prerendering_has_started());
  entry = prerender_manager()->FindAndUseEntry(pending_url);
  ASSERT_EQ(pending_prerender_contents, entry.get());
}

TEST_F(PrerenderTest, InvalidPendingPrerenderTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));

  int child_id;
  int route_id;
  ASSERT_TRUE(prerender_contents->GetChildId(&child_id));
  ASSERT_TRUE(prerender_contents->GetRouteId(&route_id));

  // This pending URL has an unsupported scheme, and won't be able
  // to start.
  GURL pending_url("ftp://news.google.com/");

  // Schedule a pending prerender launched from the prerender.
  DummyPrerenderContents* pending_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          pending_url,
          ORIGIN_GWS_PRERENDER,
          FINAL_STATUS_UNSUPPORTED_SCHEME);
  prerender_link_manager()->OnAddPrerender(
      child_id, GetNextPrerenderID(), pending_url, kDefaultRelTypes,
      Referrer(url, network::mojom::ReferrerPolicy::kDefault), kSize, route_id);
  EXPECT_FALSE(LauncherHasRunningPrerender(child_id, last_prerender_id()));
  EXPECT_FALSE(pending_prerender_contents->prerendering_has_started());

  // Use the referring prerender.
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());

  // The pending prerender still doesn't start.
  EXPECT_FALSE(LauncherHasRunningPrerender(child_id, last_prerender_id()));
  EXPECT_FALSE(pending_prerender_contents->prerendering_has_started());
}

TEST_F(PrerenderTest, CancelPendingPrerenderTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));

  int child_id;
  int route_id;
  ASSERT_TRUE(prerender_contents->GetChildId(&child_id));
  ASSERT_TRUE(prerender_contents->GetRouteId(&route_id));

  GURL pending_url("http://news.google.com/");

  // Schedule a pending prerender launched from the prerender.
  prerender_link_manager()->OnAddPrerender(
      child_id, GetNextPrerenderID(), pending_url, kDefaultRelTypes,
      Referrer(url, network::mojom::ReferrerPolicy::kDefault), kSize, route_id);
  EXPECT_FALSE(LauncherHasRunningPrerender(child_id, last_prerender_id()));

  // Cancel the pending prerender.
  prerender_link_manager()->OnCancelPrerender(child_id, last_prerender_id());

  // Use the referring prerender.
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());

  // The pending prerender doesn't start.
  EXPECT_FALSE(LauncherHasRunningPrerender(child_id, last_prerender_id()));
}

// Tests that prerendering is cancelled when the source render view does not
// exist.  On failure, the DCHECK in CreatePrerenderContents() above should be
// triggered.
TEST_F(PrerenderTest, SourceRenderViewClosed) {
  GURL url("http://www.google.com/");
  prerender_manager()->CreateNextPrerenderContents(
      url,
      FINAL_STATUS_MANAGER_SHUTDOWN);
  prerender_link_manager()->OnAddPrerender(
      100, GetNextPrerenderID(), url, kDefaultRelTypes, Referrer(), kSize, 200);
  EXPECT_FALSE(LauncherHasRunningPrerender(100, last_prerender_id()));
}

// Tests that prerendering is cancelled when we launch a second prerender of
// the same target within a short time interval.
TEST_F(PrerenderTest, RecentlyVisited) {
  GURL url("http://www.google.com/");

  prerender_manager()->RecordNavigation(url);

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_RECENTLY_VISITED);
  EXPECT_FALSE(AddSimplePrerender(url));
  EXPECT_FALSE(prerender_contents->prerendering_has_started());
}

TEST_F(PrerenderTest, NotSoRecentlyVisited) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  GURL url("http://www.google.com/");

  prerender_manager()->RecordNavigation(url);
  tick_clock()->Advance(TimeDelta::FromMilliseconds(
      UnitTestPrerenderManager::kNavigationRecordWindowMs + 500));

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

// Tests that the prerender manager matches include the fragment.
TEST_F(PrerenderTest, FragmentMatchesTest) {
  GURL fragment_url("http://www.google.com/#test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(fragment_url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(fragment_url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(fragment_url);
  ASSERT_EQ(prerender_contents, entry.get());
}

// Tests that the prerender manager uses fragment references when matching
// prerender URLs in the case a different fragment is in both URLs.
TEST_F(PrerenderTest, FragmentsDifferTest) {
  GURL fragment_url("http://www.google.com/#test");
  GURL other_fragment_url("http://www.google.com/#other_test");

  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(fragment_url,
                                                       FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(fragment_url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());

  ASSERT_FALSE(prerender_manager()->FindEntry(other_fragment_url));

  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(fragment_url);
  ASSERT_EQ(prerender_contents, entry.get());
}

// Make sure that clearing works as expected.
TEST_F(PrerenderTest, ClearTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  prerender_manager()->ClearData(PrerenderManager::CLEAR_PRERENDER_CONTENTS);
  EXPECT_FALSE(prerender_manager()->FindEntry(url));
}

// Make sure canceling works as expected.
TEST_F(PrerenderTest, CancelAllTest) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  prerender_manager()->CancelAllPrerenders();
  EXPECT_FALSE(prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, OmniboxNotAllowedWhenDisabled) {
  DisablePrerender();
  EXPECT_FALSE(prerender_manager()->AddPrerenderFromOmnibox(
      GURL("http://www.example.com"), nullptr, gfx::Size()));
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_PRERENDERING_DISABLED, 1);
}

TEST_F(PrerenderTest, LinkRelStillAllowedWhenDisabled) {
  DisablePrerender();
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

TEST_F(PrerenderTest, LinkRelAllowedOnCellular) {
  EnablePrerender();
  GURL url("http://www.example.com");
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4G);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

TEST_F(PrerenderTest, PrerenderNotAllowedOnCellularWithExternalOrigin) {
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4G);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url,
          ORIGIN_EXTERNAL_REQUEST,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  std::unique_ptr<PrerenderHandle> prerender_handle(
      prerender_manager()->AddPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kSize)));
  EXPECT_FALSE(prerender_handle);
  EXPECT_FALSE(prerender_contents->prerendering_has_started());
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_CELLULAR_NETWORK, 1);
}

TEST_F(PrerenderTest, PrerenderAllowedForForcedCellular) {
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4G);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents = nullptr;
  std::unique_ptr<PrerenderHandle> prerender_handle;
  prerender_contents = prerender_manager()->CreateNextPrerenderContents(
      url, ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER, FINAL_STATUS_USED);
  prerender_handle = prerender_manager()->AddForcedPrerenderFromExternalRequest(
      url, content::Referrer(), nullptr, gfx::Rect(kSize));
  EXPECT_TRUE(prerender_handle);
  EXPECT_TRUE(prerender_handle->IsPrerendering());
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_EQ(prerender_contents, prerender_handle->contents());
  EXPECT_EQ(ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER,
            prerender_handle->contents()->origin());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

TEST_F(PrerenderTest, LinkManagerCancel) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

TEST_F(PrerenderTest, LinkManagerCancelThenAbandon) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, LinkManagerAbandon) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
}

TEST_F(PrerenderTest, LinkManagerAbandonThenCancel) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               last_prerender_id());

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));

  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, LinkManagerCancelTwice) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());
}

TEST_F(PrerenderTest, LinkManagerAddTwiceCancelTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  const int first_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimplePrerender(url));

  const int second_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              first_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, LinkManagerAddTwiceCancelTwiceThenAbandonTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimplePrerender(url));

  const int first_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimplePrerender(url));

  const int second_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              first_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               first_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               second_prerender_id);

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
}

// TODO(gavinp): Update this test after abandon has an effect on Prerenders,
// like shortening the timeouts.
TEST_F(PrerenderTest, LinkManagerAddTwiceAbandonTwiceUseTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimplePrerender(url));

  const int first_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimplePrerender(url));

  const int second_prerender_id = last_prerender_id();
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               first_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               second_prerender_id);

  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(prerender_contents, entry.get());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// TODO(gavinp): After abandon shortens the expire time on a Prerender,
// add a series of tests testing advancing the time by either the abandon
// or normal expire, and verifying the expected behaviour with groups
// of links.
TEST_F(PrerenderTest, LinkManagerExpireThenCancel) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_TIMED_OUT);

  EXPECT_TRUE(AddSimplePrerender(url));

  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));
  tick_clock()->Advance(prerender_manager()->config().time_to_live +
                        TimeDelta::FromSeconds(1));

  EXPECT_FALSE(IsEmptyPrerenderLinkManager());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());

  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, LinkManagerExpireThenAddAgain) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* first_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(first_prerender_contents->prerendering_has_started());
  EXPECT_FALSE(first_prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(first_prerender_contents,
            prerender_manager()->FindEntry(url));
  tick_clock()->Advance(prerender_manager()->config().time_to_live +
                        TimeDelta::FromSeconds(1));

  ASSERT_FALSE(prerender_manager()->FindEntry(url));
  DummyPrerenderContents* second_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(second_prerender_contents->prerendering_has_started());
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(url);
  ASSERT_EQ(second_prerender_contents, entry.get());
}

TEST_F(PrerenderTest, LinkManagerCancelThenAddAgain) {
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* first_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(first_prerender_contents->prerendering_has_started());
  EXPECT_FALSE(first_prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(first_prerender_contents, prerender_manager()->FindEntry(url));
  prerender_link_manager()->OnCancelPrerender(kDefaultChildId,
                                              last_prerender_id());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  EXPECT_TRUE(first_prerender_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(prerender_manager()->FindEntry(url));

  // A cancelled NoStatePrefetch is counted as a prefetch recently happened. A
  // new attempt to prefetch should return as duplicate.
  prerender_manager()->CreateNextPrerenderContents(
      url, FINAL_STATUS_MANAGER_SHUTDOWN);
  EXPECT_FALSE(AddSimplePrerender(url));
  EXPECT_FALSE(prerender_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, LinkManagerChannelClosing) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
  GURL url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, FINAL_STATUS_TIMED_OUT);

  EXPECT_TRUE(AddSimplePrerender(url));
  EXPECT_TRUE(prerender_contents->prerendering_has_started());
  EXPECT_FALSE(prerender_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(prerender_contents, prerender_manager()->FindEntry(url));

  prerender_link_manager()->OnChannelClosing(kDefaultChildId);

  tick_clock()->Advance(prerender_manager()->config().abandon_time_to_live +
                        TimeDelta::FromSeconds(1));

  EXPECT_FALSE(prerender_manager()->FindEntry(url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Creates two prerenders, one of which should be blocked by the
// max_link_concurrency; abandons both of them and waits to make sure both
// are cleared from the PrerenderLinkManager.
TEST_F(PrerenderTest, DISABLED_LinkManagerAbandonInactivePrerender) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(prerender_manager()->config().abandon_time_to_live,
            prerender_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          first_url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(first_url));
  const int first_prerender_id = last_prerender_id();

  GURL second_url("http://www.neverlaunched.com");
  EXPECT_FALSE(AddSimplePrerender(second_url));
  const int second_prerender_id = last_prerender_id();

  EXPECT_FALSE(IsEmptyPrerenderLinkManager());

  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(second_url));

  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               first_prerender_id);
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               second_prerender_id);

  tick_clock()->Advance(prerender_manager()->config().abandon_time_to_live +
                        TimeDelta::FromSeconds(1));
  EXPECT_FALSE(prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(second_url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Creates two prerenders, the second one started by the first, both of which
// should be blocked by max_concurrency; abandons both of them and waits to make
// sure both are cleared from the PrerenderLinkManager.
TEST_F(PrerenderTest, LinkManagerClearOnPendingAbandon) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(prerender_manager()->config().abandon_time_to_live,
            prerender_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          first_url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(first_url));
  const int first_prerender_id = last_prerender_id();

  int child_id;
  int route_id;
  ASSERT_TRUE(prerender_contents->GetChildId(&child_id));
  ASSERT_TRUE(prerender_contents->GetRouteId(&route_id));

  GURL pending_url("http://www.neverlaunched.com");
  prerender_link_manager()->OnAddPrerender(
      child_id, GetNextPrerenderID(), pending_url, kDefaultRelTypes,
      content::Referrer(), kSize, route_id);
  const int second_prerender_id = last_prerender_id();

  EXPECT_FALSE(IsEmptyPrerenderLinkManager());

  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(pending_url));

  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               first_prerender_id);
  prerender_link_manager()->OnAbandonPrerender(kDefaultChildId,
                                               second_prerender_id);

  tick_clock()->Advance(prerender_manager()->config().abandon_time_to_live +
                        TimeDelta::FromSeconds(1));
  EXPECT_FALSE(prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(pending_url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Creates two prerenders, one of which should be blocked by the
// max_link_concurrency; uses one after the max wait to launch, and
// ensures the second prerender does not start.
TEST_F(PrerenderTest, LinkManagerWaitToLaunchNotLaunched) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(prerender_manager()->config().max_wait_to_launch,
            prerender_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          first_url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimplePrerender(first_url));

  GURL second_url("http://www.neverlaunched.com");
  EXPECT_FALSE(AddSimplePrerender(second_url));

  EXPECT_FALSE(IsEmptyPrerenderLinkManager());

  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(second_url));

  tick_clock()->Advance(prerender_manager()->config().max_wait_to_launch +
                        TimeDelta::FromSeconds(1));
  EXPECT_EQ(prerender_contents, prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(second_url));

  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(first_url);
  EXPECT_EQ(prerender_contents, entry.get());

  EXPECT_FALSE(prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(second_url));
  EXPECT_TRUE(IsEmptyPrerenderLinkManager());
}

// Creates two prerenders, one of which should start when the first one expires.
TEST_F(PrerenderTest, LinkManagerExpireRevealingLaunch) {
  prerender_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(prerender_manager()->config().max_wait_to_launch,
            prerender_manager()->config().time_to_live);

  GURL first_url("http://www.willexpire.com");
  DummyPrerenderContents* first_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          first_url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimplePrerender(first_url));
  EXPECT_EQ(first_prerender_contents,
            prerender_manager()->FindEntry(first_url));

  // Insert the second prerender so it will be still be launchable when the
  // first expires.
  const TimeDelta wait_to_launch_second_prerender =
      prerender_manager()->config().time_to_live -
      prerender_manager()->config().max_wait_to_launch +
      TimeDelta::FromSeconds(2);
  const TimeDelta wait_for_first_prerender_to_expire =
      prerender_manager()->config().time_to_live -
      wait_to_launch_second_prerender +
      TimeDelta::FromSeconds(1);
  ASSERT_LT(prerender_manager()->config().time_to_live,
            wait_to_launch_second_prerender +
            wait_for_first_prerender_to_expire);
  ASSERT_GT(prerender_manager()->config().max_wait_to_launch.InSeconds(),
            wait_for_first_prerender_to_expire.InSeconds());

  tick_clock()->Advance(wait_to_launch_second_prerender);
  GURL second_url("http://www.willlaunch.com");
  DummyPrerenderContents* second_prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          second_url, FINAL_STATUS_USED);
  EXPECT_FALSE(AddSimplePrerender(second_url));

  // The first prerender is still running, but the second has not yet launched.
  EXPECT_EQ(first_prerender_contents,
            prerender_manager()->FindEntry(first_url));
  EXPECT_FALSE(prerender_manager()->FindEntry(second_url));

  // The first prerender should have died, giving life to the second one.
  tick_clock()->Advance(wait_for_first_prerender_to_expire);
  EXPECT_FALSE(prerender_manager()->FindEntry(first_url));
  std::unique_ptr<PrerenderContents> entry =
      prerender_manager()->FindAndUseEntry(second_url);
  EXPECT_EQ(second_prerender_contents, entry.get());
}

TEST_F(PrerenderTest, PrerenderContentsIsValidHttpMethod) {
  EXPECT_TRUE(IsValidHttpMethod(FULL_PRERENDER, "GET"));
  EXPECT_TRUE(IsValidHttpMethod(FULL_PRERENDER, "HEAD"));
  EXPECT_TRUE(IsValidHttpMethod(FULL_PRERENDER, "OPTIONS"));
  EXPECT_TRUE(IsValidHttpMethod(FULL_PRERENDER, "POST"));
  EXPECT_TRUE(IsValidHttpMethod(FULL_PRERENDER, "TRACE"));
  EXPECT_FALSE(IsValidHttpMethod(FULL_PRERENDER, "WHATEVER"));

  EXPECT_TRUE(IsValidHttpMethod(PREFETCH_ONLY, "GET"));
  EXPECT_TRUE(IsValidHttpMethod(PREFETCH_ONLY, "HEAD"));
  EXPECT_FALSE(IsValidHttpMethod(PREFETCH_ONLY, "OPTIONS"));
  EXPECT_FALSE(IsValidHttpMethod(PREFETCH_ONLY, "POST"));
  EXPECT_FALSE(IsValidHttpMethod(PREFETCH_ONLY, "TRACE"));
  EXPECT_FALSE(IsValidHttpMethod(PREFETCH_ONLY, "WHATEVER"));
}

TEST_F(PrerenderTest, PrerenderContentsIncrementsByteCount) {
  GURL url("http://www.google.com/");
  DummyPrerenderContents* prerender_contents =
      prerender_manager()->CreateNextPrerenderContents(
          url, ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER,
          FINAL_STATUS_MANAGER_SHUTDOWN);
  std::unique_ptr<PrerenderHandle> prerender_handle =
      prerender_manager()->AddForcedPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kSize));

  TestNetworkBytesChangedObserver observer;
  prerender_handle->SetObserver(&observer);

  prerender_contents->AddNetworkBytes(12);
  EXPECT_TRUE(observer.network_bytes_changed());
  EXPECT_EQ(12, prerender_contents->network_bytes());
}

}  // namespace prerender
