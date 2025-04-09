// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

namespace contextual_cueing {

namespace {

using ::testing::_;

constexpr char kFooURL[] = "https://foo.com";
constexpr char kBarURL[] = "https://bar.com";
constexpr char kBazURL[] = "https://baz.com";
constexpr char kQuxURL[] = "https://qux.com";

class ContextualCueingServiceTest : public testing::Test {
 public:
  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "24h"},
           {"BackoffMultiplierBase", "2.0"},
           {"NudgeCapTime", "24h"},
           {"NudgeCapCount", "3"},
           {"MinPageCountBetweenNudges", "0"}}}},
        /*disabled_features=*/{});
  }

  void SetUp() override {
    InitializeFeatureList();
    service_ = std::make_unique<ContextualCueingService>(
        &page_content_extraction_service_,
        /*optimization_guide_keyed_service=*/nullptr,
        /*loading_predictor=*/nullptr,
        /*pref_service=*/nullptr);
  }

  ContextualCueingService* service() { return service_.get(); }

  void FastForwardBy(base::TimeDelta time_delta) {
    task_environment_.FastForwardBy(time_delta);
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  page_content_annotations::PageContentExtractionService
      page_content_extraction_service_;
  std::unique_ptr<ContextualCueingService> service_;
};

class ContextualCueingServiceTestCapCountAndMinPageCount
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"NudgeCapCount", "3"}, {"MinPageCountBetweenNudges", "3"}}}},
        /*disabled_features=*/{});
  }
};

// Tests the maximum nudge cap per 24 hours, and the minimum page counts needed
// to show the next nudge. Does not test the backoff logic.
TEST_F(ContextualCueingServiceTestCapCountAndMinPageCount,
       AllowsNudgeCapCountAndMinPageCountBetweenNudges) {
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBazURL));
  FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    FastForwardBy(base::Minutes(1));
  }

  // 3 cues allowed within 24 hours.
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  FastForwardBy(base::Hours(25));
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, AllowsNudge) {
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByBackoffRule) {
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  FastForwardBy(base::Hours(13));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  FastForwardBy(base::Hours(12));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBarURL));
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  FastForwardBy(base::Minutes(48 * 60 + 1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBazURL));
  service()->CueingNudgeDismissed();  // Backoff time is 96 hours.
  FastForwardBy(base::Minutes(96 * 60 - 1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  FastForwardBy(base::Minutes(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, BackoffCountResetAfterClick) {
  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  FastForwardBy(base::Hours(25));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBarURL));
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  FastForwardBy(base::Hours(49));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBazURL));
  service()->CueingNudgeClicked();  // Backoff count resets.
  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.

  FastForwardBy(base::Hours(23));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeHasElapsedSinceLastNudge);
  FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByFrequency) {
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Hours(1));
  service()->CueingNudgeShown(GURL(kBarURL));
  FastForwardBy(base::Hours(4));
  service()->CueingNudgeShown(GURL(kBazURL));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  FastForwardBy(base::Hours(18));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);
  FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kFooURL));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);
}

class ContextualCueingServiceTestMinPageCountBetweenNudges
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0"},
           {"NudgeCapTime", "0h"},
           {"MinPageCountBetweenNudges", "3"}}}},
        /*disabled_features=*/{});
  }
};

TEST_F(ContextualCueingServiceTestMinPageCountBetweenNudges,
       MinPageCountBetweenNudges) {
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Minutes(1));

  // 3 quiet page loads after the cue.
  for (size_t i = 0; i < 3; i++) {
    service()->ReportPageLoad();
    EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
              NudgeDecision::kNotEnoughPageLoadsSinceLastNudge);
    FastForwardBy(base::Minutes(1));
  }

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  FastForwardBy(base::Minutes(1));
}

// Tests the per domain nudge limits, ie., x nudges per y hours for each domain.
class ContextualCueingServiceTestPerDomainLimits
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"BackoffTime", "0h"},
           {"BackoffMultiplierBase", "0"},
           {"NudgeCapTime", "100h"},
           {"NudgeCapCount", "100"},
           {"MinPageCountBetweenNudges", "0"},
           {"NudgeCapTimePerDomain", "24h"},
           {"NudgeCapCountPerDomain", "1"}}}},
        /*disabled_features=*/{});
  }
};

TEST_F(ContextualCueingServiceTestPerDomainLimits, PerDomainLimits) {
  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Minutes(1));

  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kTooManyNudgesShownToTheUserForDomain);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->ReportPageLoad();
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  FastForwardBy(base::Minutes(1));

  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kTooManyNudgesShownToTheUserForDomain);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kTooManyNudgesShownToTheUserForDomain);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);

  FastForwardBy(base::Hours(24));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);
}

class MockLoadingPredictor : public predictors::LoadingPredictor {
 public:
  explicit MockLoadingPredictor(Profile* profile)
      : LoadingPredictor(predictors::LoadingPredictorConfig(), profile) {}

  MOCK_METHOD(void,
              PreconnectURLIfAllowed,
              (const GURL& url,
               bool allow_credentials,
               const net::NetworkAnonymizationKey& network_anonymization_key,
               const net::NetworkTrafficAnnotationTag& traffic_annotation,
               const content::StoragePartitionConfig* storage_partition_config),
              (override));
};

#if BUILDFLAG(ENABLE_GLIC)
class ContextualCueingServiceTestZeroStateSuggestions : public testing::Test {
 public:
  ContextualCueingServiceTestZeroStateSuggestions() {
    scoped_feature_list_.InitAndEnableFeature(kGlicZeroStateSuggestions);
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        optimization_guide::switches::
            kOptimizationGuideServiceModelExecutionURL,
        "https://mes.com/");

    web_contents_factory_ = std::make_unique<content::TestWebContentsFactory>();

    loading_predictor_ =
        std::make_unique<testing::NiceMock<MockLoadingPredictor>>(&profile_);

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    glic::prefs::RegisterProfilePrefs(pref_service_->registry());
    service_ = std::make_unique<ContextualCueingService>(
        /*page_content_extraction_service=*/nullptr,
        /*optimization_guide_keyed_service=*/nullptr, loading_predictor_.get(),
        pref_service_.get());
  }

  void TearDown() override {
    loading_predictor_->Shutdown();
  }

  void SetGlicTabContextEnabled(bool enabled) {
    pref_service_->SetBoolean(glic::prefs::kGlicTabContextEnabled, enabled);
  }

  ContextualCueingService* service() { return service_.get(); }

  MockLoadingPredictor* loading_predictor() { return loading_predictor_.get(); }

  content::WebContents* CreateWebContents() {
    return web_contents_factory_->CreateWebContents(&profile_);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  std::unique_ptr<content::TestWebContentsFactory> web_contents_factory_;
  std::unique_ptr<testing::NiceMock<MockLoadingPredictor>> loading_predictor_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<ContextualCueingService> service_;
};

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       PreconnectsWithContextEnabled) {
  EXPECT_CALL(*loading_predictor(),
              PreconnectURLIfAllowed(GURL("https://mes.com/"), _, _, _, _));

  SetGlicTabContextEnabled(true);
  service()->PrepareToFetchContextualGlicZeroStateSuggestions(
      CreateWebContents());
}

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       NoPreconnectWhenContextDisabled) {
  EXPECT_CALL(*loading_predictor(), PreconnectURLIfAllowed).Times(0);

  SetGlicTabContextEnabled(false);
  service()->PrepareToFetchContextualGlicZeroStateSuggestions(
      CreateWebContents());
}

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       InitializesPageDataWithContextEnabled) {
  SetGlicTabContextEnabled(true);

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  content::WebContents* web_contents = CreateWebContents();
  service()->GetContextualGlicZeroStateSuggestions(
      web_contents, /*is_fre=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());

  EXPECT_NE(nullptr, ZeroStateSuggestionsPageData::GetForPage(
                         web_contents->GetPrimaryPage()));
}

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       DoesNotInitializePageDataWithContextDisabled) {
  SetGlicTabContextEnabled(false);

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  content::WebContents* web_contents = CreateWebContents();
  service()->GetContextualGlicZeroStateSuggestions(
      web_contents, /*is_fre=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(nullptr, ZeroStateSuggestionsPageData::GetForPage(
                         web_contents->GetPrimaryPage()));
}
#endif

}  // namespace

}  // namespace contextual_cueing
