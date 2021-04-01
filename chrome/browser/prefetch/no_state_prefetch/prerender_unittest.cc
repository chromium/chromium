// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/metrics/field_trial_param_associator.h"
#include "base/metrics/field_trial_params.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/net/prediction_options.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/prefetch/no_state_prefetch/chrome_no_state_prefetch_manager_delegate.h"
#include "chrome/browser/prefetch/no_state_prefetch/prerender_test_utils.h"
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
#include "components/no_state_prefetch/common/prerender_origin.h"
#include "components/no_state_prefetch/common/prerender_util.h"
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
using base::TimeDelta;
using base::TimeTicks;
using content::Referrer;

namespace prerender {

class UnitTestNoStatePrefetchManager;

namespace {

class DummyNoStatePrefetchContents : public NoStatePrefetchContents {
 public:
  DummyNoStatePrefetchContents(
      UnitTestNoStatePrefetchManager* test_no_state_prefetch_manager,
      const GURL& url,
      Origin origin,
      const base::Optional<url::Origin>& initiator_origin,
      FinalStatus expected_final_status);

  ~DummyNoStatePrefetchContents() override;

  void StartPrerendering(
      const gfx::Rect& bounds,
      content::SessionStorageNamespace* session_storage_namespace) override;

  FinalStatus expected_final_status() const { return expected_final_status_; }

  bool prerendering_has_been_cancelled() const {
    return NoStatePrefetchContents::prerendering_has_been_cancelled();
  }

 private:
  static int g_next_route_id_;
  int route_id_;

  UnitTestNoStatePrefetchManager* test_no_state_prefetch_manager_;
  FinalStatus expected_final_status_;
};

class TestNetworkBytesChangedObserver
    : public prerender::NoStatePrefetchHandle::Observer {
 public:
  TestNetworkBytesChangedObserver() : network_bytes_changed_(false) {}

  // prerender::NoStatePrefetchHandle::Observer
  void OnPrefetchStop(
      NoStatePrefetchHandle* no_state_prefetch_handle) override {}
  void OnPrefetchNetworkBytesChanged(
      NoStatePrefetchHandle* no_state_prefetch_handle) override {
    network_bytes_changed_ = true;
  }

  bool network_bytes_changed() const { return network_bytes_changed_; }

 private:
  bool network_bytes_changed_;

  DISALLOW_COPY_AND_ASSIGN(TestNetworkBytesChangedObserver);
};

int DummyNoStatePrefetchContents::g_next_route_id_ = 0;

const gfx::Size kDefaultViewSize(640, 480);

}  // namespace

class UnitTestNoStatePrefetchManager : public NoStatePrefetchManager {
 public:
  using NoStatePrefetchManager::kNavigationRecordWindowMs;

  explicit UnitTestNoStatePrefetchManager(Profile* profile)
      : NoStatePrefetchManager(
            profile,
            std::make_unique<ChromeNoStatePrefetchManagerDelegate>(profile)) {
    set_rate_limit_enabled(false);
  }

  ~UnitTestNoStatePrefetchManager() override {}

  // From KeyedService, via PrererenderManager:
  void Shutdown() override {
    if (next_no_state_prefetch_contents())
      next_no_state_prefetch_contents_->Destroy(FINAL_STATUS_PROFILE_DESTROYED);
    NoStatePrefetchManager::Shutdown();
  }

  // From NoStatePrefetchManager:
  void MoveEntryToPendingDelete(NoStatePrefetchContents* entry,
                                FinalStatus final_status) override {
    if (entry == next_no_state_prefetch_contents_.get())
      return;
    NoStatePrefetchManager::MoveEntryToPendingDelete(entry, final_status);
  }

  NoStatePrefetchContents* FindEntry(const GURL& url) {
    DeleteOldEntries();
    to_delete_prefetches_.clear();
    NoStatePrefetchData* data = FindNoStatePrefetchData(url, nullptr);
    return data ? data->contents() : nullptr;
  }

  std::unique_ptr<NoStatePrefetchContents> FindAndUseEntry(const GURL& url) {
    NoStatePrefetchData* no_state_prefetch_data =
        FindNoStatePrefetchData(url, nullptr);
    if (!no_state_prefetch_data)
      return nullptr;
    auto to_erase = FindIteratorForNoStatePrefetchContents(
        no_state_prefetch_data->contents());
    CHECK(to_erase != active_prefetches_.end());
    std::unique_ptr<NoStatePrefetchContents> no_state_prefetch_contents =
        no_state_prefetch_data->ReleaseContents();
    active_prefetches_.erase(to_erase);

    no_state_prefetch_contents->MarkAsUsedForTesting();
    return no_state_prefetch_contents;
  }

  DummyNoStatePrefetchContents* CreateNextNoStatePrefetchContents(
      const GURL& url,
      FinalStatus expected_final_status) {
    return SetNextNoStatePrefetchContents(
        std::make_unique<DummyNoStatePrefetchContents>(
            this, url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN,
            url::Origin::Create(GURL("https://uniquedifferentorigin.com")),
            expected_final_status));
  }

  DummyNoStatePrefetchContents* CreateNextNoStatePrefetchContents(
      const GURL& url,
      const base::Optional<url::Origin>& initiator_origin,
      Origin origin,
      FinalStatus expected_final_status) {
    return SetNextNoStatePrefetchContents(
        std::make_unique<DummyNoStatePrefetchContents>(
            this, url, origin, initiator_origin, expected_final_status));
  }

  DummyNoStatePrefetchContents* CreateNextNoStatePrefetchContents(
      const GURL& url,
      const std::vector<GURL>& alias_urls,
      FinalStatus expected_final_status) {
    auto no_state_prefetch_contents =
        std::make_unique<DummyNoStatePrefetchContents>(
            this, url, ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN,
            url::Origin::Create(GURL("https://uniquedifferentorigin.com")),
            expected_final_status);
    for (const GURL& alias : alias_urls)
      EXPECT_TRUE(no_state_prefetch_contents->AddAliasURL(alias));
    return SetNextNoStatePrefetchContents(
        std::move(no_state_prefetch_contents));
  }

  void set_rate_limit_enabled(bool enabled) {
    mutable_config().rate_limit_enabled = enabled;
  }

  NoStatePrefetchContents* next_no_state_prefetch_contents() {
    return next_no_state_prefetch_contents_.get();
  }

  NoStatePrefetchContents* GetNoStatePrefetchContentsForRoute(
      int child_id,
      int route_id) const override {
    // Overridden for the NoStatePrefetchLinkManager's pending prefetch logic.
    auto it = no_state_prefetch_contents_map_.find(
        std::make_pair(child_id, route_id));
    return it != no_state_prefetch_contents_map_.end() ? it->second : nullptr;
  }

  void DummyNoStatePrefetchContentsStarted(
      int child_id,
      int route_id,
      NoStatePrefetchContents* no_state_prefetch_contents) {
    no_state_prefetch_contents_map_[std::make_pair(child_id, route_id)] =
        no_state_prefetch_contents;
  }

  void DummyNoStatePrefetchContentsDestroyed(int child_id, int route_id) {
    no_state_prefetch_contents_map_.erase(std::make_pair(child_id, route_id));
  }

  void SetIsLowEndDevice(bool is_low_end_device) {
    is_low_end_device_ = is_low_end_device;
  }

 private:
  bool IsLowEndDevice() const override { return is_low_end_device_; }

  DummyNoStatePrefetchContents* SetNextNoStatePrefetchContents(
      std::unique_ptr<DummyNoStatePrefetchContents>
          no_state_prefetch_contents) {
    CHECK(!next_no_state_prefetch_contents_);
    DummyNoStatePrefetchContents* contents_ptr =
        no_state_prefetch_contents.get();
    next_no_state_prefetch_contents_ = std::move(no_state_prefetch_contents);
    return contents_ptr;
  }

  std::unique_ptr<NoStatePrefetchContents> CreateNoStatePrefetchContents(
      const GURL& url,
      const Referrer& referrer,
      const base::Optional<url::Origin>& initiator_origin,
      Origin origin) override {
    CHECK(next_no_state_prefetch_contents_);
    EXPECT_EQ(url, next_no_state_prefetch_contents_->prerender_url());
    EXPECT_EQ(origin, next_no_state_prefetch_contents_->origin());
    return std::move(next_no_state_prefetch_contents_);
  }

  // Maintain a map from route pairs to NoStatePrefetchContents for
  // GetNoStatePrefetchContentsForRoute.
  using NoStatePrefetchContentsMap =
      std::map<std::pair<int, int>, NoStatePrefetchContents*>;
  NoStatePrefetchContentsMap no_state_prefetch_contents_map_;

  std::unique_ptr<NoStatePrefetchContents> next_no_state_prefetch_contents_;
  bool is_low_end_device_;
};

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

DummyNoStatePrefetchContents::DummyNoStatePrefetchContents(
    UnitTestNoStatePrefetchManager* test_no_state_prefetch_manager,
    const GURL& url,
    Origin origin,
    const base::Optional<url::Origin>& initiator_origin,
    FinalStatus expected_final_status)
    : NoStatePrefetchContents(
          std::make_unique<ChromeNoStatePrefetchContentsDelegate>(),
          test_no_state_prefetch_manager,
          nullptr,
          url,
          Referrer(),
          initiator_origin,
          origin),
      route_id_(g_next_route_id_++),
      test_no_state_prefetch_manager_(test_no_state_prefetch_manager),
      expected_final_status_(expected_final_status) {}

DummyNoStatePrefetchContents::~DummyNoStatePrefetchContents() {
  EXPECT_EQ(expected_final_status_, final_status());
  test_no_state_prefetch_manager_->DummyNoStatePrefetchContentsDestroyed(
      -1, route_id_);
}

void DummyNoStatePrefetchContents::StartPrerendering(
    const gfx::Rect& bounds,
    content::SessionStorageNamespace* session_storage_namespace) {
  load_start_time_ = test_no_state_prefetch_manager_->GetCurrentTimeTicks();
  prerendering_has_started_ = true;
  test_no_state_prefetch_manager_->DummyNoStatePrefetchContentsStarted(
      -1, route_id_, this);
  NotifyPrefetchStart();
}

class PrerenderTest : public testing::Test {
 public:
  static const int kDefaultChildId = -1;
  static const int kDefaultRenderViewRouteId = -1;

  PrerenderTest()
      : no_state_prefetch_manager_(
            new UnitTestNoStatePrefetchManager(&profile_)),
        no_state_prefetch_link_manager_(
            new NoStatePrefetchLinkManager(no_state_prefetch_manager_.get())) {
    no_state_prefetch_manager()->SetIsLowEndDevice(false);
  }

  ~PrerenderTest() override {
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
                      int render_view_id) {
    auto attributes = blink::mojom::PrerenderAttributes::New();
    attributes->url = url;
    attributes->rel_type = blink::mojom::PrerenderRelType::kPrerender;
    attributes->referrer = blink::mojom::Referrer::New(
        initiator_url, network::mojom::ReferrerPolicy::kDefault);
    attributes->view_size = kDefaultViewSize;

    // This could delete an existing prefetcher as a side-effect.
    base::Optional<int> link_trigger_id =
        no_state_prefetch_link_manager()->OnStartLinkTrigger(
            render_process_id, render_view_id, std::move(attributes),
            url::Origin::Create(initiator_url));

    // Check if the new prefetch request was added and running.
    return link_trigger_id && LastTriggerIsRunning();
  }

  // Shorthand to add a simple link trigger with a reasonable source. Returns
  // true iff the prefetcher has been added to the NoStatePrefetchManager by the
  // NoStatePrefetchLinkManager and the NoStatePrefetchManager returned a
  // handle.
  bool AddSimpleLinkTrigger(const GURL& url) {
    return AddLinkTrigger(url, GURL(), kDefaultChildId,
                          kDefaultRenderViewRouteId);
  }

  // Shorthand to add a simple link trigger with a reasonable source. Returns
  // true iff the prefetcher has been added to the NoStatePrefetchManager by the
  // NoStatePrefetchLinkManager and the NoStatePrefetchManager returned a
  // handle. The referrer is set to a google domain.
  bool AddSimpleGWSLinkTrigger(const GURL& url) {
    return AddLinkTrigger(url, GURL("https://www.google.com"), kDefaultChildId,
                          kDefaultRenderViewRouteId);
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
  // Needed to pass NoStatePrefetchManager's DCHECKs.
  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<UnitTestNoStatePrefetchManager> no_state_prefetch_manager_;
  std::unique_ptr<NoStatePrefetchLinkManager> no_state_prefetch_link_manager_;
  base::HistogramTester histogram_tester_;
};

TEST_F(PrerenderTest, RespectsThirdPartyCookiesPref) {
  GURL url("http://www.google.com/");
  profile()->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(content_settings::CookieControlsMode::kBlockThirdParty));
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  histogram_tester().ExpectUniqueSample(
      "Prerender.FinalStatus", FINAL_STATUS_BLOCK_THIRD_PARTY_COOKIES, 1);
}

TEST_F(PrerenderTest, GWSPrefetchHoldbackNonGWSSReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGWSPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

TEST_F(PrerenderTest, GWSPrefetchHoldbackGWSReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kGWSPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.google.com")), ORIGIN_GWS_PRERENDER,
      FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_FALSE(AddSimpleGWSLinkTrigger(url));
}

TEST_F(PrerenderTest, GWSPrefetchHoldbackOffNonGWSReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kGWSPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

TEST_F(PrerenderTest, GWSPrefetchHoldbackOffGWSReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kGWSPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.google.com")), ORIGIN_GWS_PRERENDER,
      FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleGWSLinkTrigger(url));
}

TEST_F(PrerenderTest, PredictorPrefetchHoldbackNonPredictorReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kNavigationPredictorPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.notgoogle.com")),
      ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

TEST_F(PrerenderTest, PredictorPrefetchHoldbackPredictorReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kNavigationPredictorPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, base::nullopt, ORIGIN_NAVIGATION_PREDICTOR,
      FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_EQ(nullptr,
            no_state_prefetch_manager()->AddPrerenderFromNavigationPredictor(
                url, nullptr, gfx::Size()));
}

// Verify that link-rel:next URLs are not prefetched.
TEST_F(PrerenderTest, LinkRelNextWithNSPDisabled) {
  GURL url("http://www.notgoogle.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.notgoogle.com")), ORIGIN_LINK_REL_NEXT,
      FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_EQ(
      nullptr,
      no_state_prefetch_manager()->AddPrerenderWithPreconnectFallbackForTesting(
          ORIGIN_LINK_REL_NEXT, url,
          url::Origin::Create(GURL("www.notgoogle.com"))));
  histogram_tester().ExpectUniqueSample(
      "Prerender.FinalStatus", FINAL_STATUS_LINK_REL_NEXT_NOT_ALLOWED, 1);
}

TEST_F(PrerenderTest, PredictorPrefetchHoldbackOffNonPredictorReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kNavigationPredictorPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, url::Origin::Create(GURL("www.notgoogle.com")),
      ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));
}

TEST_F(PrerenderTest, PredictorPrefetchHoldbackOffPredictorReferrer) {
  GURL url("http://www.notgoogle.com/");
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kNavigationPredictorPrefetchHoldback);
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, base::nullopt, ORIGIN_NAVIGATION_PREDICTOR,
      FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_NE(nullptr,
            no_state_prefetch_manager()->AddPrerenderFromNavigationPredictor(
                url, nullptr, gfx::Size()));
}

// Flaky on Android and Mac, crbug.com/1087876.
TEST_F(PrerenderTest, DISABLED_PrerenderDisabledOnLowEndDevice) {
  GURL url("http://www.google.com/");
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_LOW_END_DEVICE, 1);
}

TEST_F(PrerenderTest, FoundTest) {
  base::TimeDelta prefetch_age;
  FinalStatus final_status;
  Origin origin;

  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());

  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());

  EXPECT_TRUE(no_state_prefetch_manager()->GetPrefetchInformation(
      url, &prefetch_age, &final_status, &origin));
  EXPECT_EQ(prerender::FINAL_STATUS_UNKNOWN, final_status);
  EXPECT_EQ(prerender::ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, origin);

  const base::TimeDelta advance_duration = TimeDelta::FromSeconds(1);
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
TEST_F(PrerenderTest, DISABLED_DuplicateTest_NoStatePrefetch) {
  SetConcurrency(2);
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());

  DummyNoStatePrefetchContents* no_state_prefetch_contents1 =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_EQ(no_state_prefetch_contents1,
            no_state_prefetch_manager()->next_no_state_prefetch_contents());
  EXPECT_FALSE(no_state_prefetch_contents1->prerendering_has_started());

  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Ensure that we expire a prerendered page after the max. permitted time.
TEST_F(PrerenderTest, ExpireTest) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  tick_clock()->Advance(no_state_prefetch_manager()->config().time_to_live +
                        TimeDelta::FromSeconds(1));
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Ensure that we don't launch prerenders of bad urls (in this case, a mailto:
// url)
TEST_F(PrerenderTest, BadURLTest) {
  GURL url("mailto:test@gmail.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_UNSUPPORTED_SCHEME);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// When the user navigates away from a page, the prerenders it launched should
// have their time to expiry shortened from the default time to live.
TEST_F(PrerenderTest, LinkManagerNavigateAwayExpire) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  const TimeDelta time_to_live = TimeDelta::FromSeconds(300);
  const TimeDelta abandon_time_to_live = TimeDelta::FromSeconds(20);
  const TimeDelta test_advance = TimeDelta::FromSeconds(22);
  ASSERT_LT(test_advance, time_to_live);
  ASSERT_LT(abandon_time_to_live, test_advance);

  no_state_prefetch_manager()->mutable_config().time_to_live = time_to_live;
  no_state_prefetch_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
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
TEST_F(PrerenderTest, LinkManagerNavigateAwayNearExpiry) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
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

  no_state_prefetch_manager()->mutable_config().time_to_live = time_to_live;
  no_state_prefetch_manager()->mutable_config().abandon_time_to_live =
      abandon_time_to_live;

  GURL url("http://example2.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
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
TEST_F(PrerenderTest, LinkManagerNavigateAwayLaunchAnother) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  const TimeDelta time_to_live = TimeDelta::FromSeconds(300);
  const TimeDelta abandon_time_to_live = TimeDelta::FromSeconds(20);
  const TimeDelta test_advance = TimeDelta::FromSeconds(5);
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
  DummyNoStatePrefetchContents* second_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          second_url, FINAL_STATUS_PROFILE_DESTROYED);
  EXPECT_TRUE(AddSimpleLinkTrigger(second_url));
  EXPECT_EQ(second_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(second_url));
}

// Prefetching the same URL twice during |time_to_live| results in a duplicate
// and is aborted.
TEST_F(PrerenderTest, NoStatePrefetchDuplicate) {
  const GURL kUrl("http://www.google.com/");
  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());

  // Prefetch the url once.
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      kUrl, base::nullopt, ORIGIN_OMNIBOX, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(no_state_prefetch_manager()->AddPrerenderFromOmnibox(
      kUrl, nullptr, gfx::Size()));
  // Cancel the prerender so that it is not reused.
  no_state_prefetch_manager()->CancelAllPrerenders();

  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      kUrl, base::nullopt, ORIGIN_OMNIBOX, FINAL_STATUS_PROFILE_DESTROYED);

  // Prefetching again before time_to_live aborts, because it is a duplicate.
  tick_clock()->Advance(base::TimeDelta::FromSeconds(1));
  EXPECT_FALSE(no_state_prefetch_manager()->AddPrerenderFromOmnibox(
      kUrl, nullptr, gfx::Size()));
  histogram_tester().ExpectBucketCount("Prerender.FinalStatus",
                                       FINAL_STATUS_DUPLICATE, 1);

  // Prefetching after time_to_live succeeds.
  tick_clock()->Advance(
      base::TimeDelta::FromMinutes(net::HttpCache::kPrefetchReuseMins));
  EXPECT_TRUE(no_state_prefetch_manager()->AddPrerenderFromOmnibox(
      kUrl, nullptr, gfx::Size()));
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
          no_state_prefetch_contentses.back()->prerendering_has_started());
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
    DummyNoStatePrefetchContents* no_state_prefetch_contents_to_delay =
        no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
            url_to_delay, FINAL_STATUS_USED);
    EXPECT_FALSE(AddSimpleLinkTrigger(url_to_delay));
    EXPECT_FALSE(
        no_state_prefetch_contents_to_delay->prerendering_has_started());
    EXPECT_TRUE(no_state_prefetch_manager()->next_no_state_prefetch_contents());
    EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url_to_delay));
    for (size_t j = 0; j < effective_max_link_concurrency; ++j) {
      std::unique_ptr<NoStatePrefetchContents> entry =
          no_state_prefetch_manager()->FindAndUseEntry(urls[j]);
      EXPECT_EQ(no_state_prefetch_contentses[j], entry.get());
      EXPECT_TRUE(
          no_state_prefetch_contents_to_delay->prerendering_has_started());
    }

    std::unique_ptr<NoStatePrefetchContents> entry =
        no_state_prefetch_manager()->FindAndUseEntry(url_to_delay);
    EXPECT_EQ(no_state_prefetch_contents_to_delay, entry.get());
    EXPECT_FALSE(
        no_state_prefetch_manager()->next_no_state_prefetch_contents());
  }
}

// Flaky on Android: https://crbug.com/1105908
TEST_F(PrerenderTest, DISABLED_AliasURLTest) {
  SetConcurrency(7);

  GURL url("http://www.google.com/");
  GURL alias_url1("http://www.google.com/index.html");
  GURL alias_url2("http://google.com/");
  GURL not_an_alias_url("http://google.com/index.html");
  std::vector<GURL> alias_urls;
  alias_urls.push_back(alias_url1);
  alias_urls.push_back(alias_url2);

  // Test that all of the aliases work, but not_an_alias_url does not.
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
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
TEST_F(PrerenderTest, SourceRenderViewClosed) {
  GURL url("http://www.google.com/");
  no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
      url, FINAL_STATUS_PROFILE_DESTROYED);
  AddLinkTrigger(url, url, 100, 200);
  EXPECT_FALSE(LastTriggerExists());
}

// Tests that prerendering is cancelled when we launch a second prerender of
// the same target within a short time interval.
TEST_F(PrerenderTest, RecentlyVisited) {
  GURL url("http://www.google.com/");

  no_state_prefetch_manager()->RecordNavigation(url);

  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_RECENTLY_VISITED);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_started());
}

TEST_F(PrerenderTest, NotSoRecentlyVisited) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  GURL url("http://www.google.com/");

  no_state_prefetch_manager()->RecordNavigation(url);
  tick_clock()->Advance(TimeDelta::FromMilliseconds(
      UnitTestNoStatePrefetchManager::kNavigationRecordWindowMs + 500));

  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Tests that the prerender manager matches include the fragment.
TEST_F(PrerenderTest, FragmentMatchesTest) {
  GURL fragment_url("http://www.google.com/#test");

  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          fragment_url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(fragment_url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(fragment_url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Tests that the prerender manager uses fragment references when matching
// prerender URLs in the case a different fragment is in both URLs.
TEST_F(PrerenderTest, FragmentsDifferTest) {
  GURL fragment_url("http://www.google.com/#test");
  GURL other_fragment_url("http://www.google.com/#other_test");

  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          fragment_url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(fragment_url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());

  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(other_fragment_url));

  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(fragment_url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Make sure that clearing works as expected.
TEST_F(PrerenderTest, ClearTest) {
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CACHE_OR_HISTORY_CLEARED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  no_state_prefetch_manager()->ClearData(
      NoStatePrefetchManager::CLEAR_PRERENDER_CONTENTS);
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Make sure canceling works as expected.
TEST_F(PrerenderTest, CancelAllTest) {
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  no_state_prefetch_manager()->CancelAllPrerenders();
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Test that when prerender is enabled, a prerender initiated by omnibox is
// successful.
TEST_F(PrerenderTest, OmniboxAllowedWhenNotDisabled) {
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          GURL("http://www.example.com"), base::nullopt, ORIGIN_OMNIBOX,
          FINAL_STATUS_PROFILE_DESTROYED);

  EXPECT_TRUE(no_state_prefetch_manager()->AddPrerenderFromOmnibox(
      GURL("http://www.example.com"), nullptr, gfx::Size()));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
}

// Test that when prerender fails and the
// kPrerenderFallbackToPreconnect experiment is not enabled,
// a prerender initiated by omnibox does not result in a preconnect.
TEST_F(PrerenderTest, OmniboxAllowedWhenNotDisabled_LowMemory_FeatureDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kPrerenderFallbackToPreconnect);
  const GURL kURL(GURL("http://www.example.com"));
  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  // Prerender should be disabled on low memory devices.
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(no_state_prefetch_manager()->AddPrerenderFromOmnibox(
      kURL, nullptr, gfx::Size()));

  EXPECT_EQ(0u, loading_predictor->GetActiveHintsSizeForTesting());
}

// Test that when prerender fails and the
// kPrerenderFallbackToPreconnect experiment is enabled, a
// prerender initiated by omnibox actually results in preconnect.
TEST_F(PrerenderTest, Omnibox_AllowedWhenNotDisabled_LowMemory_FeatureEnabled) {
  const GURL kURL(GURL("http://www.example.com"));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrerenderFallbackToPreconnect);

  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  // Prerender should be disabled on low memory devices.
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(no_state_prefetch_manager()->AddPrerenderFromOmnibox(
      kURL, nullptr, gfx::Size()));

  // Verify that the prerender request falls back to a preconnect request.
  EXPECT_EQ(1u, loading_predictor->GetActiveHintsSizeForTesting());

  auto& active_hints = loading_predictor->active_hints_for_testing();
  auto it = active_hints.find(kURL);
  EXPECT_NE(it, active_hints.end());
}

// Test that when prerender fails and the
// kPrerenderFallbackToPreconnect experiment is enabled, a
// prerender initiated by an external request actually results in preconnect.
TEST_F(PrerenderTest,
       ExternalRequest_AllowedWhenNotDisabled_LowMemory_FeatureEnabled) {
  const GURL kURL(GURL("http://www.example.com"));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrerenderFallbackToPreconnect);

  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  // Prerender should be disabled on low memory devices.
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(no_state_prefetch_manager()->AddPrerenderFromExternalRequest(
      kURL, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize)));

  // Verify that the prerender request falls back to a preconnect request.
  EXPECT_EQ(1u, loading_predictor->GetActiveHintsSizeForTesting());

  auto& active_hints = loading_predictor->active_hints_for_testing();
  auto it = active_hints.find(kURL);
  EXPECT_NE(it, active_hints.end());
}

// Test that when prerender fails and the
// kPrerenderFallbackToPreconnect experiment is enabled, a
// prerender initiated by isolated prerender does not trigger a preconnect.
TEST_F(PrerenderTest, IsolatedPrerenderDoesNotPreconnect) {
  const GURL kURL(GURL("http://www.example.com"));
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPrerenderFallbackToPreconnect);

  predictors::LoadingPredictorConfig config;
  PopulateTestConfig(&config);

  auto* loading_predictor =
      predictors::LoadingPredictorFactory::GetForProfile(profile());
  loading_predictor->StartInitialization();
  content::RunAllTasksUntilIdle();

  // Prerender should be disabled on low memory devices.
  no_state_prefetch_manager()->SetIsLowEndDevice(true);
  EXPECT_FALSE(no_state_prefetch_manager()->AddIsolatedPrerender(
      kURL, nullptr, kDefaultViewSize));

  // Verify that the prerender request does not fall back to a preconnect.
  EXPECT_EQ(0u, loading_predictor->GetActiveHintsSizeForTesting());
}

TEST_F(PrerenderTest, LinkRelStillAllowedWhenDisabled) {
  DisablePrerender();
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, url::Origin::Create(GURL("https://www.notgoogle.com")),
          ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(PrerenderTest, LinkRelAllowedOnCellular) {
  EnablePrerender();
  GURL url("http://www.example.com");
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4GMetered);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_COST_METERED,
            net::NetworkChangeNotifier::GetConnectionCost());
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, url::Origin::Create(GURL("https://www.notexample.com")),
          ORIGIN_LINK_REL_PRERENDER_CROSSDOMAIN, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

// Verify that the external prerender requests are not allowed on cellular
// connection when kPredictivePrefetchingAllowedOnAllConnectionTypes feature is
// not enabled.
TEST_F(PrerenderTest, PrerenderNotAllowedOnCellularWithExternalOrigin) {
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4GMetered);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_COST_METERED,
            net::NetworkChangeNotifier::GetConnectionCost());
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, base::nullopt, ORIGIN_EXTERNAL_REQUEST,
          FINAL_STATUS_PROFILE_DESTROYED);
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle(
      no_state_prefetch_manager()->AddPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize)));
  EXPECT_TRUE(no_state_prefetch_handle);
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  histogram_tester().ExpectTotalCount("Prerender.FinalStatus", 0);
}

// Verify that the external prerender requests are allowed on unmetered cellular
// connection when kPredictivePrefetchingAllowedOnAllConnectionTypes feature is
// not enabled.
TEST_F(PrerenderTest, PrerenderAllowedOnUnmeteredCellularWithExternalOrigin) {
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4GUnmetered);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_COST_UNMETERED,
            net::NetworkChangeNotifier::GetConnectionCost());
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, base::nullopt, ORIGIN_EXTERNAL_REQUEST,
          FINAL_STATUS_PROFILE_DESTROYED);
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle(
      no_state_prefetch_manager()->AddPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize)));
  EXPECT_TRUE(no_state_prefetch_handle);
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  histogram_tester().ExpectTotalCount("Prerender.FinalStatus", 0);
}

// Verify that the external prerender requests are not allowed on metered wifi
// connection when kPredictivePrefetchingAllowedOnAllConnectionTypes feature is
// not enabled.
TEST_F(PrerenderTest, PrerenderNotAllowedOnMeteredWifiWithExternalOrigin) {
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifierWifiMetered);
  EXPECT_FALSE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  EXPECT_EQ(net::NetworkChangeNotifier::CONNECTION_COST_METERED,
            net::NetworkChangeNotifier::GetConnectionCost());
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, base::nullopt, ORIGIN_EXTERNAL_REQUEST,
          FINAL_STATUS_PROFILE_DESTROYED);
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle(
      no_state_prefetch_manager()->AddPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize)));
  EXPECT_TRUE(no_state_prefetch_handle);
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  histogram_tester().ExpectTotalCount("Prerender.FinalStatus", 0);
}

// Verify that the external prerender requests are allowed on cellular
// connection when kPredictivePrefetchingAllowedOnAllConnectionTypes feature is
// enabled.
TEST_F(
    PrerenderTest,
    PrerenderAllowedOnCellularWithExternalOrigin_PredictivePrefetchingAllowedOnAllConnectionTypes) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      features::kPredictivePrefetchingAllowedOnAllConnectionTypes);
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4GMetered);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, base::nullopt, ORIGIN_EXTERNAL_REQUEST, FINAL_STATUS_USED);
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle(
      no_state_prefetch_manager()->AddPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize)));
  EXPECT_TRUE(no_state_prefetch_handle);
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_EQ(no_state_prefetch_contents, no_state_prefetch_handle->contents());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(PrerenderTest, PrerenderAllowedForForcedCellular) {
  EnablePrerender();
  std::unique_ptr<net::NetworkChangeNotifier> mock(
      new MockNetworkChangeNotifier4GMetered);
  EXPECT_TRUE(net::NetworkChangeNotifier::IsConnectionCellular(
      net::NetworkChangeNotifier::GetConnectionType()));
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents = nullptr;
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle;
  no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, base::nullopt, ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER,
          FINAL_STATUS_USED);
  no_state_prefetch_handle =
      no_state_prefetch_manager()->AddForcedPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize));
  EXPECT_TRUE(no_state_prefetch_handle);
  EXPECT_TRUE(no_state_prefetch_handle->IsPrefetching());
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_EQ(no_state_prefetch_contents, no_state_prefetch_handle->contents());
  EXPECT_EQ(ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER,
            no_state_prefetch_handle->contents()->origin());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(PrerenderTest, LinkManagerCancel) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());
  CancelLastTrigger();

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

TEST_F(PrerenderTest, LinkManagerAbandon) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());
  AbandonLastTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
}

TEST_F(PrerenderTest, LinkManagerAbandonThenCancel) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());
  AbandonLastTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));

  CancelLastTrigger();
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// Flaky on Android, crbug.com/1087876.
// Flaky on Mac and Linux, crbug.com/1087735.
#if defined(OS_ANDROID) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#define MAYBE_LinkManagerAddTwiceCancelTwice \
  DISABLED_LinkManagerAddTwiceCancelTwice
#else
#define MAYBE_LinkManagerAddTwiceCancelTwice LinkManagerAddTwiceCancelTwice
#endif
TEST_F(PrerenderTest, MAYBE_LinkManagerAddTwiceCancelTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  CancelFirstTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  EXPECT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  CancelFirstTrigger();

  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

// TODO(gavinp): Update this test after abandon has an effect on Prerenders,
// like shortening the timeouts.
// Flaky on Android and Linux, crbug.com/1087876 & crbug.com/1087736.
TEST_F(PrerenderTest, DISABLED_LinkManagerAddTwiceAbandonTwiceUseTwice) {
  SetConcurrency(2);
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  AbandonFirstTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  AbandonFirstTrigger();

  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(no_state_prefetch_contents, entry.get());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

// TODO(gavinp): After abandon shortens the expire time on a Prerender,
// add a series of tests testing advancing the time by either the abandon
// or normal expire, and verifying the expected behaviour with groups
// of links.
TEST_F(PrerenderTest, LinkManagerExpireThenCancel) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);

  EXPECT_TRUE(AddSimpleLinkTrigger(url));

  EXPECT_TRUE(no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  tick_clock()->Advance(no_state_prefetch_manager()->config().time_to_live +
                        TimeDelta::FromSeconds(1));

  EXPECT_FALSE(IsEmptyNoStatePrefetchLinkManager());

  // FindEntry will have a side-effect of pruning expired prerenders.
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));

  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
}

TEST_F(PrerenderTest, LinkManagerExpireThenAddAgain) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* first_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(first_no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(
      first_no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  tick_clock()->Advance(no_state_prefetch_manager()->config().time_to_live +
                        TimeDelta::FromSeconds(1));

  ASSERT_FALSE(no_state_prefetch_manager()->FindEntry(url));
  DummyNoStatePrefetchContents* second_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_USED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(second_no_state_prefetch_contents->prerendering_has_started());
  std::unique_ptr<NoStatePrefetchContents> entry =
      no_state_prefetch_manager()->FindAndUseEntry(url);
  ASSERT_EQ(second_no_state_prefetch_contents, entry.get());
}

// Flaky on Android, crbug.com/1087876.
TEST_F(PrerenderTest, DISABLED_LinkManagerCancelThenAddAgain) {
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  GURL url("http://www.myexample.com");
  DummyNoStatePrefetchContents* first_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, FINAL_STATUS_CANCELLED);
  EXPECT_TRUE(AddSimpleLinkTrigger(url));
  EXPECT_TRUE(first_no_state_prefetch_contents->prerendering_has_started());
  EXPECT_FALSE(
      first_no_state_prefetch_contents->prerendering_has_been_cancelled());
  ASSERT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(url));
  CancelLastTrigger();
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
  EXPECT_TRUE(
      first_no_state_prefetch_contents->prerendering_has_been_cancelled());
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
TEST_F(PrerenderTest, DISABLED_LinkManagerAbandonInactivePrerender) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(no_state_prefetch_manager()->config().abandon_time_to_live,
            no_state_prefetch_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
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
      TimeDelta::FromSeconds(1));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(first_url));
  EXPECT_FALSE(no_state_prefetch_manager()->FindEntry(second_url));
  EXPECT_TRUE(IsEmptyNoStatePrefetchLinkManager());
}

// Creates two prerenders, one of which should be blocked by the
// max_link_concurrency; uses one after the max wait to launch, and
// ensures the second prerender does not start.
TEST_F(PrerenderTest, LinkManagerWaitToLaunchNotLaunched) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(no_state_prefetch_manager()->config().max_wait_to_launch,
            no_state_prefetch_manager()->config().time_to_live);
  GURL first_url("http://www.myexample.com");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
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
      TimeDelta::FromSeconds(1));
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
TEST_F(PrerenderTest, LinkManagerExpireRevealingLaunch) {
  no_state_prefetch_manager()->SetTickClockForTesting(tick_clock());
  SetConcurrency(1);
  ASSERT_LT(no_state_prefetch_manager()->config().max_wait_to_launch,
            no_state_prefetch_manager()->config().time_to_live);

  GURL first_url("http://www.willexpire.com");
  DummyNoStatePrefetchContents* first_no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          first_url, FINAL_STATUS_TIMED_OUT);
  EXPECT_TRUE(AddSimpleLinkTrigger(first_url));
  EXPECT_EQ(first_no_state_prefetch_contents,
            no_state_prefetch_manager()->FindEntry(first_url));

  // Insert the second prerender so it will be still be launchable when the
  // first expires.
  const TimeDelta wait_to_launch_second_prerender =
      no_state_prefetch_manager()->config().time_to_live -
      no_state_prefetch_manager()->config().max_wait_to_launch +
      TimeDelta::FromSeconds(2);
  const TimeDelta wait_for_first_prerender_to_expire =
      no_state_prefetch_manager()->config().time_to_live -
      wait_to_launch_second_prerender + TimeDelta::FromSeconds(1);
  ASSERT_LT(
      no_state_prefetch_manager()->config().time_to_live,
      wait_to_launch_second_prerender + wait_for_first_prerender_to_expire);
  ASSERT_GT(
      no_state_prefetch_manager()->config().max_wait_to_launch.InSeconds(),
      wait_for_first_prerender_to_expire.InSeconds());

  tick_clock()->Advance(wait_to_launch_second_prerender);
  GURL second_url("http://www.willlaunch.com");
  DummyNoStatePrefetchContents* second_no_state_prefetch_contents =
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

TEST_F(PrerenderTest, NoStatePrefetchContentsIsValidHttpMethod) {
  EXPECT_TRUE(IsValidHttpMethod("GET"));
  EXPECT_TRUE(IsValidHttpMethod("HEAD"));
  EXPECT_FALSE(IsValidHttpMethod("OPTIONS"));
  EXPECT_FALSE(IsValidHttpMethod("POST"));
  EXPECT_FALSE(IsValidHttpMethod("TRACE"));
  EXPECT_FALSE(IsValidHttpMethod("WHATEVER"));
}

TEST_F(PrerenderTest, NoStatePrefetchContentsIncrementsByteCount) {
  GURL url("http://www.google.com/");
  DummyNoStatePrefetchContents* no_state_prefetch_contents =
      no_state_prefetch_manager()->CreateNextNoStatePrefetchContents(
          url, base::nullopt, ORIGIN_EXTERNAL_REQUEST_FORCED_PRERENDER,
          FINAL_STATUS_PROFILE_DESTROYED);
  std::unique_ptr<NoStatePrefetchHandle> no_state_prefetch_handle =
      no_state_prefetch_manager()->AddForcedPrerenderFromExternalRequest(
          url, content::Referrer(), nullptr, gfx::Rect(kDefaultViewSize));

  TestNetworkBytesChangedObserver observer;
  no_state_prefetch_handle->SetObserver(&observer);

  no_state_prefetch_contents->AddNetworkBytes(12);
  EXPECT_TRUE(observer.network_bytes_changed());
  EXPECT_EQ(12, no_state_prefetch_contents->network_bytes());
}

TEST_F(PrerenderTest, NoPrerenderInSingleProcess) {
  GURL url("http://www.google.com/");
  auto* command_line = base::CommandLine::ForCurrentProcess();
  ASSERT_TRUE(command_line != nullptr);
  command_line->AppendSwitch(switches::kSingleProcess);
  EXPECT_FALSE(AddSimpleLinkTrigger(url));
  histogram_tester().ExpectUniqueSample("Prerender.FinalStatus",
                                        FINAL_STATUS_SINGLE_PROCESS, 1);
}

}  // namespace prerender
