// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/glic_pref_names.h"
#endif

namespace contextual_cueing {

using ::testing::_;
using ::testing::An;
using ::testing::ElementsAre;
using ::testing::WithArgs;

namespace {

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
           {"MinPageCountBetweenNudges", "0"},
           {"MinTimeBetweenNudges", "30s"}}}},
        {contextual_cueing::kGlicZeroStateSuggestions});
  }

  void SetUp() override {
    InitializeFeatureList();
    mock_optimization_guide_keyed_service_ =
        std::make_unique<MockOptimizationGuideKeyedService>();
  }

  void InitializeContextualCueingService() {
    service_ = std::make_unique<ContextualCueingService>(
        &page_content_extraction_service_,
        mock_optimization_guide_keyed_service_.get(),
        /*loading_predictor=*/nullptr,
        /*pref_service=*/nullptr,
        /*template_url_service=*/nullptr);
  }

  ContextualCueingService* service() { return service_.get(); }

  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }

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
  std::unique_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  std::unique_ptr<ContextualCueingService> service_;
};

class ContextualCueingServiceTestCapCountAndMinPageCount
    : public ContextualCueingServiceTest {
  void InitializeFeatureList() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{contextual_cueing::kContextualCueing,
          {{"NudgeCapCount", "3"},
           {"MinPageCountBetweenNudges", "3"},
           {"MinTimeBetweenNudges", "0h"}}}},
        /*disabled_features=*/{});
  }
};

// Tests the maximum nudge cap per 24 hours, and the minimum page counts needed
// to show the next nudge. Does not test the backoff logic.
TEST_F(ContextualCueingServiceTestCapCountAndMinPageCount,
       AllowsNudgeCapCountAndMinPageCountBetweenNudges) {
  InitializeContextualCueingService();
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
  InitializeContextualCueingService();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgeBlockedByCooldownTime) {
  InitializeContextualCueingService();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Seconds(29));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kNotEnoughTimeSinceLastNudgeShown);
  FastForwardBy(base::Seconds(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, DoesNotRegisterOptimizationType) {
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              RegisterOptimizationTypes(ElementsAre(
                  optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS)))
      .Times(0);
  InitializeContextualCueingService();
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByBackoffRule) {
  InitializeContextualCueingService();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kFooURL));
  service()->CueingNudgeDismissed();  // Backoff time is 24 hours.
  FastForwardBy(base::Minutes(1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed);
  FastForwardBy(base::Hours(12));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)),
            NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed);
  FastForwardBy(base::Hours(12));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBarURL));
  service()->CueingNudgeDismissed();  // Backoff time is 48 hours.
  FastForwardBy(base::Minutes(1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)),
            NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed);
  FastForwardBy(base::Hours(48));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBazURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kBazURL));
  service()->CueingNudgeDismissed();  // Backoff time is 96 hours.
  FastForwardBy(base::Hours(95));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)),
            NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed);
  FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, BackoffCountResetAfterClick) {
  InitializeContextualCueingService();
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
            NudgeDecision::kNotEnoughTimeSinceLastNudgeDismissed);
  FastForwardBy(base::Hours(2));
  EXPECT_EQ(service()->CanShowNudge(GURL(kBarURL)), NudgeDecision::kSuccess);
}

TEST_F(ContextualCueingServiceTest, NudgesCappedByFrequency) {
  InitializeContextualCueingService();
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Hours(1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBarURL));
  FastForwardBy(base::Hours(4));
  EXPECT_EQ(service()->CanShowNudge(GURL(kFooURL)), NudgeDecision::kSuccess);
  service()->CueingNudgeShown(GURL(kBazURL));
  FastForwardBy(base::Minutes(1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);

  FastForwardBy(base::Hours(18));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)),
            NudgeDecision::kTooManyNudgesShownToTheUser);
  FastForwardBy(base::Hours(1));
  EXPECT_EQ(service()->CanShowNudge(GURL(kQuxURL)), NudgeDecision::kSuccess);

  service()->CueingNudgeShown(GURL(kFooURL));
  FastForwardBy(base::Minutes(1));
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
           {"MinPageCountBetweenNudges", "3"},
           {"MinTimeBetweenNudges", "0h"}}}},
        /*disabled_features=*/{});
  }
};

TEST_F(ContextualCueingServiceTestMinPageCountBetweenNudges,
       MinPageCountBetweenNudges) {
  InitializeContextualCueingService();
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
           {"MinTimeBetweenNudges", "0h"},
           {"NudgeCapTimePerDomain", "24h"},
           {"NudgeCapCountPerDomain", "1"}}}},
        /*disabled_features=*/{});
  }
};

TEST_F(ContextualCueingServiceTestPerDomainLimits, PerDomainLimits) {
  InitializeContextualCueingService();
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

    // Set up web contents with URLs of https page type.
    web_contents_ = std::unique_ptr<content::WebContents>(
        content::WebContentsTester::CreateTestWebContents(
            content::WebContents::CreateParams(&profile_)));
    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL("https://foo.com/"));

    mock_optimization_guide_keyed_service_ = static_cast<
        MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                &profile_,
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockOptimizationGuideKeyedService>();
                })));

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        optimization_guide::switches::
            kDisableCheckingUserPermissionsForTesting);
    ON_CALL(mock_optimization_guide_keyed_service(),
            CanApplyOptimization(
                _, optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS,
                An<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillByDefault(WithArgs<2>(
            [](optimization_guide::OptimizationGuideDecisionCallback callback) {
              std::move(callback).Run(
                  optimization_guide::OptimizationGuideDecision::kFalse,
                  optimization_guide::OptimizationMetadata());
            }));

    loading_predictor_ =
        std::make_unique<testing::NiceMock<MockLoadingPredictor>>(&profile_);

    pref_service_ =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    glic::prefs::RegisterProfilePrefs(pref_service_->registry());
  }

  void TearDown() override {
    loading_predictor_->Shutdown();
  }

  void SetGlicTabContextEnabled(bool enabled) {
    pref_service_->SetBoolean(glic::prefs::kGlicTabContextEnabled, enabled);
  }

  void InitializeContextualCueingService() {
    service_ = std::make_unique<ContextualCueingService>(
        /*page_content_extraction_service=*/nullptr,
        mock_optimization_guide_keyed_service_, loading_predictor_.get(),
        pref_service_.get(), /*template_url_service=*/nullptr);
  }

  ContextualCueingService* service() { return service_.get(); }

  MockLoadingPredictor* loading_predictor() { return loading_predictor_.get(); }

  MockOptimizationGuideKeyedService& mock_optimization_guide_keyed_service() {
    return *mock_optimization_guide_keyed_service_;
  }

  content::WebContents* web_contents() { return web_contents_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler enabler;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfile profile_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<testing::NiceMock<MockLoadingPredictor>> loading_predictor_;
  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> pref_service_;
  std::unique_ptr<ContextualCueingService> service_;
};

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       PreconnectsWithContextEnabled) {
  EXPECT_CALL(*loading_predictor(),
              PreconnectURLIfAllowed(GURL("https://mes.com/"), _, _, _, _));

  SetGlicTabContextEnabled(true);
  InitializeContextualCueingService();
  service()->PrepareToFetchContextualGlicZeroStateSuggestions(web_contents());
}

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       NoPreconnectWhenContextDisabled) {
  EXPECT_CALL(*loading_predictor(), PreconnectURLIfAllowed).Times(0);

  SetGlicTabContextEnabled(false);
  InitializeContextualCueingService();
  service()->PrepareToFetchContextualGlicZeroStateSuggestions(web_contents());
}

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       InitializesPageDataWithContextEnabled) {
  base::HistogramTester histogram_tester;
  SetGlicTabContextEnabled(true);
  EXPECT_CALL(mock_optimization_guide_keyed_service(),
              RegisterOptimizationTypes(ElementsAre(
                  optimization_guide::proto::GLIC_ZERO_STATE_SUGGESTIONS)))
      .Times(1);
  InitializeContextualCueingService();

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  service()->GetContextualGlicZeroStateSuggestions(
      web_contents(), /*is_fre=*/false, future.GetCallback());

  EXPECT_NE(nullptr, ZeroStateSuggestionsPageData::GetForPage(
                         web_contents()->GetPrimaryPage()));
}

TEST_F(ContextualCueingServiceTestZeroStateSuggestions,
       DoesNotInitializePageDataWithContextDisabled) {
  base::HistogramTester histogram_tester;
  SetGlicTabContextEnabled(false);
  InitializeContextualCueingService();

  base::test::TestFuture<std::optional<std::vector<std::string>>> future;
  service()->GetContextualGlicZeroStateSuggestions(
      web_contents(), /*is_fre=*/false, future.GetCallback());

  ASSERT_TRUE(future.Wait());

  histogram_tester.ExpectTotalCount(
      "ContextualCueing.ZeroStateSuggestions.ContextExtractionDone", 0);

  EXPECT_EQ(nullptr, ZeroStateSuggestionsPageData::GetForPage(
                         web_contents()->GetPrimaryPage()));
}
#endif

}  // namespace

}  // namespace contextual_cueing
