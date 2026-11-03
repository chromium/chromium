// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/page_content_annotations/core/page_category_classifier_bridge.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/test/test_sync_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/image_model.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace contextual_search {
namespace {

using ::page_content_annotations::CategoryType;
using ::page_content_annotations::PageContentAnnotationsResult;

class FakePageCategoryClassifierBridge
    : public page_content_annotations::PageCategoryClassifierBridge {
 public:
  void SetDemandActive(bool active) override { demand_active_ = active; }
  bool demand_active() const { return demand_active_; }

 private:
  bool demand_active_ = false;
};

class FakeCueTarget : public contextual_cueing::CueTarget {
 public:
  contextual_cueing::CueTargetType GetType() const override {
    return contextual_cueing::CueTargetType::kContextualSearch;
  }
  bool RequiresModelExecution() const override { return true; }
  bool IsEligible() const override { return true; }
  void CheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                        contextual_cueing::CueIntrusiveness intrusiveness,
                        EligibilityCallback callback) override {}
  bool IsPageEligible(
      const page_content_annotations::PageContentAnnotationsResult& result,
      content::WebContents* active_web_contents) const override {
    if (!active_web_contents ||
        result.GetType() !=
            page_content_annotations::AnnotationType::kCategoryClassifier) {
      return false;
    }
    for (const auto& category : result.GetCategoryResults()) {
      if (category.category_type == CategoryType::kEducation &&
          category.score > 0.7f) {
        return true;
      }
    }
    return false;
  }
  void OnAnchoredMessageClicked(contextual_cueing::CueActionData) override {}
  void OnEditPrompt(contextual_cueing::CueActionData) override {}
  ui::ImageModel GetAnchoredMessageIcon() const override {
    return ui::ImageModel();
  }
  ui::ImageModel GetOmniboxChipIcon() const override {
    return ui::ImageModel();
  }
  contextual_cueing::CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCue&,
      std::vector<tabs::TabHandle>) const override {
    return std::monostate{};
  }
  optimization_guide::proto::ContextualCueingSurface GetSurface()
      const override {
    return optimization_guide::proto::
        CONTEXTUAL_CUEING_SURFACE_CONTEXTUAL_SEARCH;
  }

  base::WeakPtr<contextual_cueing::CueTarget> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeCueTarget> weak_ptr_factory_{this};
};

class ContextualSearchCueTabStateTest
    : public ChromeRenderViewHostTestHarness {
 public:
  explicit ContextualSearchCueTabStateTest(bool enable_v2_multi_source = true)
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {contextual_cueing::kContextualCueingV2, {}},
    };
    std::vector<base::test::FeatureRef> disabled_features;
    if (enable_v2_multi_source) {
      enabled_features.push_back(
          {contextual_cueing::kContextualCueingV2MultiSource,
           {{"ContextualCueingV2MultiSourceAnnotationTimeout", "3s"}}});
    } else {
      disabled_features.push_back(
          contextual_cueing::kContextualCueingV2MultiSource);
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                disabled_features);
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {
        TestingProfile::TestingFactory{
            PageContentAnnotationsServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return page_content_annotations::
                  TestPageContentAnnotationsService::Create(
                      /*optimization_guide_model_provider=*/nullptr,
                      /*history_service=*/nullptr);
            })},
        TestingProfile::TestingFactory{
            HistoryServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return nullptr;
            })},
        TestingProfile::TestingFactory{
            SyncServiceFactory::GetInstance(),
            base::BindRepeating([](content::BrowserContext* context)
                                    -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            })}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    auto bridge = std::make_unique<FakePageCategoryClassifierBridge>();
    classifier_bridge_ = bridge.get();
    PageContentAnnotationsServiceFactory::GetForProfile(profile())
        ->SetPageCategoryClassifierBridge(std::move(bridge));
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    EXPECT_CALL(*mock_tab_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(*mock_tab_, GetContents())
        .WillRepeatedly(testing::Return(web_contents()));
    EXPECT_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(user_data_host_));

    target_ = std::make_unique<FakeCueTarget>();
    cue_tab_state_ = std::make_unique<ContextualSearchCueTabState>(*mock_tab_);
  }

  void TearDown() override {
    target_.reset();
    cue_tab_state_.reset();
    classifier_bridge_ = nullptr;
    mock_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  syncer::TestSyncService* GetTestSyncService() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  void SetHistorySyncEnabled(bool enabled) {
    GetTestSyncService()->GetUserSettings()->SetSelectedType(
        syncer::UserSelectableType::kHistory, enabled);
    GetTestSyncService()->FireStateChanged();
  }

  bool IsEligible() {
    base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
        future;
    cue_tab_state_->CheckEligibility(contextual_cueing::CueIntrusiveness::kLoud,
                                     future.GetCallback(),
                                     target_->GetWeakPtr());
    const auto& [eligible, unused_generator] = future.Get();
    return eligible;
  }

  PageContentAnnotationsResult CreateEligibleResult() {
    return PageContentAnnotationsResult::CreateCategoryResults(
        {{CategoryType::kEducation, 0.9f}});
  }

  PageContentAnnotationsResult CreateIneligibleResult() {
    return PageContentAnnotationsResult::CreateCategoryResults(
        {{CategoryType::kEducation, 0.3f}});
  }

  page_content_annotations::HistoryVisit CreateVisit(const GURL& url) {
    page_content_annotations::HistoryVisit visit(base::Time::Now(), url);
    if (web_contents()->GetController().GetLastCommittedEntry()) {
      visit.nav_entry_timestamp = web_contents()
                                      ->GetController()
                                      .GetLastCommittedEntry()
                                      ->GetTimestamp();
    }
    return visit;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<FakePageCategoryClassifierBridge> classifier_bridge_ = nullptr;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<FakeCueTarget> target_;
  std::unique_ptr<ContextualSearchCueTabState> cue_tab_state_;
};

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_NoAnnotationService) {
  cue_tab_state_->SetAnnotationServiceForTesting(nullptr);
  EXPECT_FALSE(IsEligible());
}

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_CacheHit_Eligible) {
  const GURL url("https://example.com/edu");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());

  EXPECT_TRUE(IsEligible());
}

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_CacheHit_Ineligible) {
  const GURL url("https://example.com/low");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateIneligibleResult());

  EXPECT_FALSE(IsEligible());
}

TEST_F(ContextualSearchCueTabStateTest,
       CheckEligibility_CacheMiss_AnnotationArrives) {
  const GURL url("https://example.com/pending");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_tab_state_->CheckEligibility(contextual_cueing::CueIntrusiveness::kLoud,
                                   future.GetCallback(), target_->GetWeakPtr());
  EXPECT_FALSE(future.IsReady());

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());
  const auto& [eligible, unused_generator] = future.Get();
  EXPECT_TRUE(eligible);
}

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_CacheMiss_Timeout) {
  const GURL url("https://example.com/timeout");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_tab_state_->CheckEligibility(contextual_cueing::CueIntrusiveness::kLoud,
                                   future.GetCallback(), target_->GetWeakPtr());
  EXPECT_FALSE(future.IsReady());

  task_environment()->FastForwardBy(base::Seconds(4));

  ASSERT_TRUE(future.IsReady());
  const auto& [eligible, unused_generator] = future.Get();
  EXPECT_FALSE(eligible);
}

TEST_F(ContextualSearchCueTabStateTest,
       CheckEligibility_DisablingHistorySyncCancelsPendingAndClearsCache) {
  const GURL url("https://example.com/edu");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_tab_state_->CheckEligibility(contextual_cueing::CueIntrusiveness::kLoud,
                                   future.GetCallback(), target_->GetWeakPtr());
  EXPECT_FALSE(future.IsReady());

  SetHistorySyncEnabled(false);
  const auto& [eligible, unused_generator] = future.Get();
  EXPECT_FALSE(eligible);

  EXPECT_FALSE(IsEligible());

  SetHistorySyncEnabled(true);
  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());
  EXPECT_TRUE(IsEligible());

  SetHistorySyncEnabled(false);
  SetHistorySyncEnabled(true);

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      trailing_future;
  cue_tab_state_->CheckEligibility(contextual_cueing::CueIntrusiveness::kLoud,
                                   trailing_future.GetCallback(),
                                   target_->GetWeakPtr());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(trailing_future.IsReady());

  SetHistorySyncEnabled(false);
  const auto& [trailing_eligible, unused_trailing_generator] =
      trailing_future.Get();
  EXPECT_FALSE(trailing_eligible);
}

TEST_F(ContextualSearchCueTabStateTest,
       CategoryClassifierObservation_TogglesWithHistorySync) {
  EXPECT_TRUE(classifier_bridge_->demand_active());
  EXPECT_TRUE(GetTestSyncService()->HasObserver(cue_tab_state_.get()));

  SetHistorySyncEnabled(false);
  EXPECT_FALSE(classifier_bridge_->demand_active());

  SetHistorySyncEnabled(true);
  EXPECT_TRUE(classifier_bridge_->demand_active());

  cue_tab_state_->SetAnnotationServiceForTesting(nullptr);
  EXPECT_FALSE(classifier_bridge_->demand_active());

  cue_tab_state_->SetAnnotationServiceForTesting(
      PageContentAnnotationsServiceFactory::GetForProfile(profile()));
  EXPECT_TRUE(classifier_bridge_->demand_active());
}

TEST_F(ContextualSearchCueTabStateTest,
       CategoryClassifierObservation_InitiallyHistorySyncDisabled) {
  cue_tab_state_.reset();
  EXPECT_FALSE(classifier_bridge_->demand_active());

  SetHistorySyncEnabled(false);
  cue_tab_state_ = std::make_unique<ContextualSearchCueTabState>(*mock_tab_);
  EXPECT_FALSE(classifier_bridge_->demand_active());
  EXPECT_TRUE(GetTestSyncService()->HasObserver(cue_tab_state_.get()));

  SetHistorySyncEnabled(true);
  EXPECT_TRUE(classifier_bridge_->demand_active());
}

TEST_F(ContextualSearchCueTabStateTest,
       CategoryClassifierObservation_SyncShutdownUnregistersObserver) {
  EXPECT_TRUE(classifier_bridge_->demand_active());
  EXPECT_TRUE(GetTestSyncService()->HasObserver(cue_tab_state_.get()));

  GetTestSyncService()->Shutdown();
  EXPECT_FALSE(classifier_bridge_->demand_active());
  EXPECT_FALSE(GetTestSyncService()->HasObserver(cue_tab_state_.get()));

  SetHistorySyncEnabled(true);
  EXPECT_FALSE(classifier_bridge_->demand_active());
}

class ContextualSearchCueTabStateV1Test
    : public ContextualSearchCueTabStateTest {
 public:
  ContextualSearchCueTabStateV1Test()
      : ContextualSearchCueTabStateTest(/*enable_v2_multi_source=*/false) {}
};

TEST_F(ContextualSearchCueTabStateV1Test,
       CategoryClassifierObservation_DisabledWhenV2MultiSourceOff) {
  EXPECT_FALSE(classifier_bridge_->demand_active());
  EXPECT_FALSE(GetTestSyncService()->HasObserver(cue_tab_state_.get()));

  SetHistorySyncEnabled(false);
  EXPECT_FALSE(classifier_bridge_->demand_active());

  SetHistorySyncEnabled(true);
  EXPECT_FALSE(classifier_bridge_->demand_active());
}

}  // namespace
}  // namespace contextual_search
