// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"

#include <memory>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
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
  ContextualSearchCueTabStateTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_cueing::kContextualCueingV2, {}},
            {contextual_cueing::kContextualCueingV2MultiSource,
             {{"ContextualCueingV2MultiSourceAnnotationTimeout", "3s"}}},
        },
        {});
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
            })}};
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
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
    mock_tab_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CallCheckEligibility(bool* out_eligible) {
    base::RunLoop run_loop;
    cue_tab_state_->CheckEligibility(
        contextual_cueing::CueIntrusiveness::kLoud,
        base::BindOnce(
            [](bool* out, base::OnceClosure quit, bool eligible,
               contextual_cueing::CueTarget::ContentGenerator) {
              *out = eligible;
              std::move(quit).Run();
            },
            out_eligible, run_loop.QuitClosure()),
        target_->GetWeakPtr());
    run_loop.Run();
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
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<FakeCueTarget> target_;
  std::unique_ptr<ContextualSearchCueTabState> cue_tab_state_;
};

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_NoAnnotationService) {
  cue_tab_state_->SetAnnotationServiceForTesting(nullptr);
  bool eligible = true;
  CallCheckEligibility(&eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_CacheHit_Eligible) {
  const GURL url("https://example.com/edu");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());

  bool eligible = false;
  CallCheckEligibility(&eligible);
  EXPECT_TRUE(eligible);
}

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_CacheHit_Ineligible) {
  const GURL url("https://example.com/low");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateIneligibleResult());

  bool eligible = true;
  CallCheckEligibility(&eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(ContextualSearchCueTabStateTest,
       CheckEligibility_CacheMiss_AnnotationArrives) {
  const GURL url("https://example.com/pending");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  bool eligible = false;
  bool callback_ran = false;
  cue_tab_state_->CheckEligibility(
      contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran),
      target_->GetWeakPtr());

  EXPECT_FALSE(callback_ran);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_ran; }));

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(eligible);
}

TEST_F(ContextualSearchCueTabStateTest, CheckEligibility_CacheMiss_Timeout) {
  const GURL url("https://example.com/timeout");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  bool eligible = true;
  bool callback_ran = false;
  cue_tab_state_->CheckEligibility(
      contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran),
      target_->GetWeakPtr());

  EXPECT_FALSE(callback_ran);

  task_environment()->FastForwardBy(base::Seconds(4));

  EXPECT_TRUE(callback_ran);
  EXPECT_FALSE(eligible);
}

}  // namespace
}  // namespace contextual_search
