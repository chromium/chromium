// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_anonymization_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::DoAll;
using testing::SetArgPointee;

namespace predictors {

namespace {

// First two are preconnectable, last one is not (see SetUp()).
const char kUrl[] = "http://www.google.com/cats";
const char kUrl2[] = "http://www.google.com/dogs";
const char kUrl3[] =
    "file://unknown.website/catsanddogs";  // Non http(s) scheme to avoid
                                           // preconnect to the main frame.

class MockPreconnectManager : public PreconnectManager {
 public:
  MockPreconnectManager(base::WeakPtr<Delegate> delegate, Profile* profile);

  MOCK_METHOD2(StartProxy,
               void(const GURL& url,
                    const std::vector<PreconnectRequest>& requests));
  MOCK_METHOD2(
      StartPreresolveHost,
      void(const GURL& url,
           const net::NetworkAnonymizationKey& network_anonymization_key));
  MOCK_METHOD2(
      StartPreresolveHosts,
      void(const std::vector<GURL>& urls,
           const net::NetworkAnonymizationKey& network_anonymization_key));
  MOCK_METHOD3(StartPreconnectUrl,
               void(const GURL& url,
                    bool allow_credentials,
                    net::NetworkAnonymizationKey network_anonymization_key));
  MOCK_METHOD1(Stop, void(const GURL& url));

  void Start(const GURL& url,
             std::vector<PreconnectRequest> requests) override {
    StartProxy(url, requests);
  }
};

MockPreconnectManager::MockPreconnectManager(base::WeakPtr<Delegate> delegate,
                                             Profile* profile)
    : PreconnectManager(delegate, profile) {}

LoadingPredictorConfig CreateConfig() {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  return config;
}

// Creates a NetworkAnonymizationKey for a main frame navigation to URL.
net::NetworkAnonymizationKey CreateNetworkanonymization_key(
    const GURL& main_frame_url) {
  net::SchemefulSite site = net::SchemefulSite(main_frame_url);
  return net::NetworkAnonymizationKey::CreateSameSite(site);
}

NavigationId GetNextId() {
  static NavigationId::Generator generator;
  return generator.GenerateNextId();
}

}  // namespace

class LoadingPredictorTest : public testing::Test {
 public:
  ~LoadingPredictorTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  virtual void SetPreference();

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<LoadingPredictor> predictor_;
  raw_ptr<StrictMock<MockResourcePrefetchPredictor>> mock_predictor_;
};

LoadingPredictorTest::~LoadingPredictorTest() = default;

void LoadingPredictorTest::SetUp() {
  profile_ = std::make_unique<TestingProfile>();
  SetPreference();
  auto config = CreateConfig();
  predictor_ = std::make_unique<LoadingPredictor>(config, profile_.get());

  auto mock = std::make_unique<StrictMock<MockResourcePrefetchPredictor>>(
      config, profile_.get());
  EXPECT_CALL(*mock, PredictPreconnectOrigins(GURL(kUrl), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, PredictPreconnectOrigins(GURL(kUrl2), _))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock, PredictPreconnectOrigins(GURL(kUrl3), _))
      .WillRepeatedly(Return(false));

  mock_predictor_ = mock.get();
  predictor_->set_mock_resource_prefetch_predictor(std::move(mock));

  predictor_->StartInitialization();
  content::RunAllTasksUntilIdle();
}

void LoadingPredictorTest::TearDown() {
  predictor_->Shutdown();
}

void LoadingPredictorTest::SetPreference() {
  prefetch::SetPreloadPagesState(profile_->GetPrefs(),
                                 prefetch::PreloadPagesState::kNoPreloading);
}

class LoadingPredictorPreconnectTest : public LoadingPredictorTest {
 public:
  void SetUp() override;
  void TearDown() override;

 protected:
  void SetPreference() override;

  raw_ptr<StrictMock<MockPreconnectManager>> mock_preconnect_manager_;
};

void LoadingPredictorPreconnectTest::SetUp() {
  LoadingPredictorTest::SetUp();
  auto mock_preconnect_manager =
      std::make_unique<StrictMock<MockPreconnectManager>>(
          predictor_->GetWeakPtr(), profile_.get());
  mock_preconnect_manager_ = mock_preconnect_manager.get();
  predictor_->set_mock_preconnect_manager(std::move(mock_preconnect_manager));
}

void LoadingPredictorPreconnectTest::TearDown() {
  mock_preconnect_manager_ = nullptr;
  LoadingPredictorTest::TearDown();
}

void LoadingPredictorPreconnectTest::SetPreference() {
  prefetch::SetPreloadPagesState(
      profile_->GetPrefs(), prefetch::PreloadPagesState::kStandardPreloading);
}

TEST_F(LoadingPredictorTest, TestOnNavigationStarted) {
  // Should return true if there are predictions.
  auto navigation_id = GetNextId();
  EXPECT_TRUE(predictor_->OnNavigationStarted(
      navigation_id, ukm::SourceId(), /*initiator_origin=*/std::nullopt,
      GURL(kUrl), base::TimeTicks::Now()));

  // Should return false since there are no predictions.
  auto navigation_id2 = GetNextId();
  EXPECT_FALSE(predictor_->OnNavigationStarted(
      navigation_id2, ukm::SourceId(), /*initiator_origin=*/std::nullopt,
      GURL(kUrl3), base::TimeTicks::Now()));
}

TEST_F(LoadingPredictorTest, TestMainFrameResponseCancelsHint) {
  const GURL url = GURL(kUrl);
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt, url,
                                 HintOrigin::EXTERNAL);
  EXPECT_EQ(1UL, predictor_->active_hints_.size());

  auto navigation_id = GetNextId();
  predictor_->OnNavigationFinished(navigation_id, url, url, false);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorTest, TestMainFrameResponseClearsNavigations) {
  const GURL url(kUrl);
  const GURL redirected(kUrl2);
  const auto& active_navigations = predictor_->active_navigations_;
  const auto& active_hints = predictor_->active_hints_;
  const auto& active_urls_to_navigations =
      predictor_->active_urls_to_navigations_;

  auto navigation_id = GetNextId();

  predictor_->OnNavigationStarted(navigation_id, ukm::SourceId(),
                                  /*initiator_origin=*/std::nullopt, url,
                                  base::TimeTicks::Now());
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_FALSE(active_hints.empty());
  EXPECT_NE(active_urls_to_navigations.find(url),
            active_urls_to_navigations.end());

  predictor_->OnNavigationFinished(navigation_id, url, url, false);
  EXPECT_TRUE(active_navigations.empty());
  EXPECT_TRUE(active_hints.empty());
  EXPECT_TRUE(active_urls_to_navigations.empty());

  // With redirects.
  predictor_->OnNavigationStarted(navigation_id, ukm::SourceId(),
                                  /*initiator_origin=*/std::nullopt, url,
                                  base::TimeTicks::Now());
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  EXPECT_FALSE(active_hints.empty());
  EXPECT_NE(active_urls_to_navigations.find(url),
            active_urls_to_navigations.end());

  predictor_->OnNavigationFinished(navigation_id, url, redirected, false);
  EXPECT_TRUE(active_navigations.empty());
  EXPECT_TRUE(active_hints.empty());
  EXPECT_TRUE(active_urls_to_navigations.empty());
}

TEST_F(LoadingPredictorTest, TestMainFrameRequestDoesntCancelExternalHint) {
  const GURL url = GURL(kUrl);
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt, url,
                                 HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // To check that the hint is not replaced, set the start time in the past,
  // and check later that it didn't change.
  base::TimeTicks start_time = it->second - base::Seconds(10);
  it->second = start_time;

  auto navigation_id = GetNextId();

  predictor_->OnNavigationStarted(navigation_id, ukm::SourceId(),
                                  /*initiator_origin=*/std::nullopt,
                                  GURL(url.spec()), base::TimeTicks::Now());
  EXPECT_NE(active_navigations.find(navigation_id), active_navigations.end());
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_EQ(start_time, it->second);
}

TEST_F(LoadingPredictorTest, TestDuplicateHintAfterPreconnectCompleteCalled) {
  const GURL url = GURL(kUrl);
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt, url,
                                 HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // To check that the hint is replaced, set the start time in the past,
  // and check later that it changed.
  base::TimeTicks start_time = it->second - base::Seconds(10);
  it->second = start_time;

  std::unique_ptr<PreconnectStats> preconnect_stats =
      std::make_unique<PreconnectStats>(url);
  predictor_->PreconnectFinished(std::move(preconnect_stats));

  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt, url,
                                 HintOrigin::NAVIGATION_PREDICTOR);
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // Calling PreconnectFinished() must have cleared the hint, and duplicate
  // PrepareForPageLoad() call should be honored.
  EXPECT_LT(start_time, it->second);
}

TEST_F(LoadingPredictorTest,
       TestDuplicateHintAfterPreconnectCompleteNotCalled) {
  const GURL url = GURL(kUrl);
  const auto& active_navigations = predictor_->active_navigations_;
  auto& active_hints = predictor_->active_hints_;

  predictor_->PrepareForPageLoad(std::nullopt, url, HintOrigin::EXTERNAL);
  auto it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  content::RunAllTasksUntilIdle();
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());

  // To check that the hint is not replaced, set the start time in the recent
  // past, and check later that it didn't change.
  base::TimeTicks start_time = it->second - base::Seconds(10);
  it->second = start_time;

  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt, url,
                                 HintOrigin::NAVIGATION_PREDICTOR);
  it = active_hints.find(url);
  EXPECT_NE(it, active_hints.end());
  EXPECT_TRUE(active_navigations.empty());

  // Duplicate PrepareForPageLoad() call should not be honored.
  EXPECT_EQ(start_time, it->second);
}

TEST_F(LoadingPredictorTest, TestDontTrackNonPrefetchableUrls) {
  const GURL url3 = GURL(kUrl3);
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt, url3,
                                 HintOrigin::NAVIGATION);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorTest, TestDontPredictOmniboxHints) {
  const GURL omnibox_suggestion = GURL("http://search.com/kittens");
  // We expect that no prediction will be requested.
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                 omnibox_suggestion, HintOrigin::OMNIBOX);
  EXPECT_TRUE(predictor_->active_hints_.empty());
}

TEST_F(LoadingPredictorPreconnectTest, TestHandleOmniboxHint) {
  const GURL preconnect_suggestion = GURL("http://search.com/kittens");
  EXPECT_CALL(*mock_preconnect_manager_,
              StartPreconnectUrl(
                  preconnect_suggestion, true,
                  CreateNetworkanonymization_key(preconnect_suggestion)));
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                 preconnect_suggestion, HintOrigin::OMNIBOX,
                                 true);
  // The second suggestion for the same host should be filtered out.
  const GURL preconnect_suggestion2 = GURL("http://search.com/puppies");
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                 preconnect_suggestion2, HintOrigin::OMNIBOX,
                                 true);

  const GURL preresolve_suggestion = GURL("http://en.wikipedia.org/wiki/main");
  net::SchemefulSite site = net::SchemefulSite(preresolve_suggestion);
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartPreresolveHost(preresolve_suggestion,
                          net::NetworkAnonymizationKey::CreateSameSite(site)));
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                 preresolve_suggestion, HintOrigin::OMNIBOX,
                                 false);
  // The second suggestions should be filtered out as well.
  const GURL preresolve_suggestion2 =
      GURL("http://en.wikipedia.org/wiki/random");
  predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                 preresolve_suggestion2, HintOrigin::OMNIBOX,
                                 false);
}

// Checks that the predictor preconnects to an initial origin even when it
// doesn't have any historical data for this host.
TEST_F(LoadingPredictorPreconnectTest, TestAddInitialUrlToEmptyPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(Return(false));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       CreateNetworkanonymization_key(main_frame_url)}})));
  EXPECT_FALSE(predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                              main_frame_url,
                                              HintOrigin::NAVIGATION));
}

// Checks that the predictor doesn't add an initial origin to a preconnect list
// if the list already contains the origin.
TEST_F(LoadingPredictorPreconnectTest, TestAddInitialUrlMatchesPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  net::NetworkAnonymizationKey network_anonymization_key =
      CreateNetworkanonymization_key(main_frame_url);
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://ads.search.com")), 0,
        network_anonymization_key}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://ads.search.com")), 0,
                       network_anonymization_key}})));
  EXPECT_TRUE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url, HintOrigin::EXTERNAL));
}

// Checks that the predictor adds an initial origin to a preconnect list if the
// list doesn't contain this origin already. It may be possible if an initial
// url redirects to another host.
TEST_F(LoadingPredictorPreconnectTest, TestAddInitialUrlDoesntMatchPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  net::NetworkAnonymizationKey network_anonymization_key =
      CreateNetworkanonymization_key(main_frame_url);
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://en.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://ads.search.com")), 0,
        network_anonymization_key}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://en.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://ads.search.com")), 0,
                       network_anonymization_key}})));
  EXPECT_TRUE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url, HintOrigin::EXTERNAL));
}

// Checks that the predictor doesn't preconnect to a bad url.
TEST_F(LoadingPredictorPreconnectTest, TestAddInvalidInitialUrl) {
  GURL main_frame_url("file:///tmp/index.html");
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillOnce(Return(false));
  EXPECT_FALSE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url, HintOrigin::EXTERNAL));
}

// Checks that the predictor uses the provided prediction if there isn't an
// active hint initiated via a local prediction happening already.
TEST_F(LoadingPredictorPreconnectTest,
       TestPrepareForPageLoadPredictionProvided) {
  GURL main_frame_url("http://search.com/kittens");
  net::NetworkAnonymizationKey network_anonymization_key =
      CreateNetworkanonymization_key(main_frame_url);
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://cdn1.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn2.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn3.search.com")), 1,
        network_anonymization_key}});
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://cdn1.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn2.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn3.search.com")), 1,
                       network_anonymization_key}})));
  EXPECT_TRUE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url,
      HintOrigin::OPTIMIZATION_GUIDE, false, prediction));
}

// Checks that the predictor does not proceed with an empty request.
TEST_F(LoadingPredictorPreconnectTest,
       TestPrepareForPageLoadPredictionWithEmptyRequestsProvided) {
  GURL main_frame_url("http://nopredictions.com/");
  net::NetworkAnonymizationKey network_anonymization_key =
      CreateNetworkanonymization_key(main_frame_url);
  PreconnectPrediction prediction;
  EXPECT_FALSE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url,
      HintOrigin::OPTIMIZATION_GUIDE, false, prediction));
}

// Checks that the predictor preconnects to an initial origin even when it
// doesn't have any historical data for this host, but still allows subsequent
// calls to PrepareForPageLoad with a provided prediction.
TEST_F(LoadingPredictorPreconnectTest,
       TestPrepareForPageLoadPreconnectsUsingPredictionWhenNoLocalPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillRepeatedly(Return(false));
  net::NetworkAnonymizationKey network_anonymization_key =
      CreateNetworkanonymization_key(main_frame_url);
  EXPECT_CALL(*mock_preconnect_manager_,
              StartProxy(main_frame_url,
                         std::vector<PreconnectRequest>(
                             {{url::Origin::Create(GURL("http://search.com")),
                               2, network_anonymization_key}})));
  EXPECT_FALSE(predictor_->PrepareForPageLoad(/*initiator_origin=*/std::nullopt,
                                              main_frame_url,
                                              HintOrigin::NAVIGATION));

  // A second call to PrepareForPageLoad using a provided prediction should
  // fire requests.
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://cdn1.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn2.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn3.search.com")), 1,
        network_anonymization_key}});
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://cdn1.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn2.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn3.search.com")), 1,
                       network_anonymization_key}})));
  EXPECT_TRUE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url,
      HintOrigin::OPTIMIZATION_GUIDE, false, prediction));
}

// Checks that the predictor uses a prediction even if there is already a local
// initiated one in flight.
TEST_F(
    LoadingPredictorPreconnectTest,
    TestPrepareForPageLoadPredictionProvidedButHasLocalPreconnectPrediction) {
  GURL main_frame_url("http://search.com/kittens");
  net::NetworkAnonymizationKey network_anonymization_key =
      CreateNetworkanonymization_key(main_frame_url);
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://ads.search.com")), 0,
        network_anonymization_key}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(main_frame_url, _))
      .WillRepeatedly(DoAll(SetArgPointee<1>(prediction), Return(true)));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://search.com")), 2,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://ads.search.com")), 0,
                       network_anonymization_key}})));
  EXPECT_TRUE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url, HintOrigin::EXTERNAL));

  // A second call to PrepareForPageLoad using a provided prediction should not
  // fire requests.
  prediction = CreatePreconnectPrediction(
      "search.com", true,
      {{url::Origin::Create(GURL("http://cdn1.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn2.search.com")), 1,
        network_anonymization_key},
       {url::Origin::Create(GURL("http://cdn3.search.com")), 1,
        network_anonymization_key}});
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartProxy(main_frame_url,
                 std::vector<PreconnectRequest>(
                     {{url::Origin::Create(GURL("http://cdn1.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn2.search.com")), 1,
                       network_anonymization_key},
                      {url::Origin::Create(GURL("http://cdn3.search.com")), 1,
                       network_anonymization_key}})));
  EXPECT_TRUE(predictor_->PrepareForPageLoad(
      /*initiator_origin=*/std::nullopt, main_frame_url,
      HintOrigin::OPTIMIZATION_GUIDE, false, prediction));
}

// Checks that the opaque origins will not trigger preconnect as it is treated
// as cross-origin and cannot be reused.
TEST_F(LoadingPredictorPreconnectTest, TestHandleHintWithOpaqueOrigins) {
  GURL main_frame_url("about:blank");
  LoadingPredictor::PreconnectData preconnect_data;
  EXPECT_FALSE(predictor_->HandleHintByOrigin(main_frame_url,
                                              /*preconnectable=*/true,
                                              /*only_allow_https=*/false,
                                              preconnect_data));
}

// Checks that the behavior of HandleHintByOrigin is expected when
// only_allow_https = true.
TEST_F(LoadingPredictorPreconnectTest, TestHandleHintWhenOnlyHttpsAllowed) {
  GURL main_frame_url_non_https("http://www.google.com/cats");
  GURL main_frame_url_https("https://www.google.com/cats");
  LoadingPredictor::PreconnectData preconnect_data;
  EXPECT_FALSE(predictor_->HandleHintByOrigin(main_frame_url_non_https,
                                              /*preconnectable=*/true,
                                              /*only_allow_https=*/true,
                                              preconnect_data));
  EXPECT_CALL(
      *mock_preconnect_manager_,
      StartPreconnectUrl(main_frame_url_https, true,
                         CreateNetworkanonymization_key(main_frame_url_https)));
  EXPECT_TRUE(predictor_->HandleHintByOrigin(main_frame_url_https,
                                             /*preconnectable=*/true,
                                             /*only_allow_https=*/true,
                                             preconnect_data));
}

// Checks that HandleHintByOrigin can preresolve correctly.
TEST_F(LoadingPredictorPreconnectTest,
       TestHandleHintPreresolveWhenOnlyHttpsAllowed) {
  GURL main_frame_url_non_https("http://www.google.com/cats");
  GURL main_frame_url_https("https://www.google.com/cats");
  LoadingPredictor::PreconnectData preconnect_data;
  EXPECT_FALSE(predictor_->HandleHintByOrigin(main_frame_url_non_https,
                                              /*preconnectable=*/false,
                                              /*only_allow_https=*/true,
                                              preconnect_data));
  EXPECT_CALL(*mock_preconnect_manager_,
              StartPreresolveHost(
                  main_frame_url_https,
                  CreateNetworkanonymization_key(main_frame_url_https)));
  EXPECT_TRUE(predictor_->HandleHintByOrigin(main_frame_url_https,
                                             /*preconnectable=*/false,
                                             /*only_allow_https=*/true,
                                             preconnect_data));
}

}  // namespace predictors
