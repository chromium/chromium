// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_search/contextual_search_cue_target.h"

#include <memory>
#include <variant>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_search/contextual_search_cue_tab_state.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/pdf/common/constants.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#endif

namespace contextual_search {
namespace {

using ::page_content_annotations::Category;
using ::page_content_annotations::CategoryType;
using ::page_content_annotations::PageContentAnnotationsResult;

class ContextualSearchCueTargetTest : public ChromeRenderViewHostTestHarness {
 public:
  explicit ContextualSearchCueTargetTest(bool discard_shopping_pdfs = true) {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_cueing::kContextualCueingV2,
             {{"ContextualCueingV2EduClassifierThreshold", "0.7"},
              {"ContextualCueingV2ShoppingClassifierThreshold", "0.6"},
              {"ContextualCueingV2DiscardShoppingPdfs",
               discard_shopping_pdfs ? "true" : "false"}}},
            {contextual_tasks::kContextualSearchContextualCuesHandleEdu, {}},
            {contextual_tasks::kContextualSearchContextualCuesHandleShopping,
             {}},
            {contextual_tasks::kContextualTasksForceEntryPointEligibility, {}},
        },
        {});
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                SyncServiceFactory::GetInstance(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<syncer::TestSyncService>();
                })}});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    EXPECT_CALL(*mock_tab_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
#if !BUILDFLAG(IS_ANDROID)
    mock_browser_window_interface_ =
        std::make_unique<testing::NiceMock<MockBrowserWindowInterface>>();
    ON_CALL(*mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(window_user_data_host_));
    EXPECT_CALL(*mock_tab_, GetBrowserWindowInterface())
        .WillRepeatedly(testing::Return(mock_browser_window_interface_.get()));
#endif

    target_ = std::make_unique<ContextualSearchCueTarget>(
        /*optimization_guide_keyed_service=*/nullptr, *mock_tab_);
  }

  void TearDown() override {
    target_.reset();
#if !BUILDFLAG(IS_ANDROID)
    mock_browser_window_interface_.reset();
#endif
    mock_tab_.reset();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  PageContentAnnotationsResult CreateAnnotationResult(CategoryType type,
                                                      int score_percent) {
    return PageContentAnnotationsResult::CreateCategoryResults(
        {{type, static_cast<float>(score_percent) / 100.0f}});
  }

  PageContentAnnotationsResult CreateMultiAnnotationResult(CategoryType type1,
                                                           int score1_percent,
                                                           CategoryType type2,
                                                           int score2_percent) {
    return PageContentAnnotationsResult::CreateCategoryResults(
        {{type1, static_cast<float>(score1_percent) / 100.0f},
         {type2, static_cast<float>(score2_percent) / 100.0f}});
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;

  ui::UnownedUserDataHost window_user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<testing::NiceMock<MockBrowserWindowInterface>>
      mock_browser_window_interface_;
#endif

  std::unique_ptr<ContextualSearchCueTarget> target_;
};

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualSearchCueTargetTest, IsEligible_HistorySync) {
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile()));
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin);

  // History sync off -> Ineligible.
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, false);
  EXPECT_FALSE(target_->IsEligible());

  // History sync on -> Eligible.
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);
  EXPECT_TRUE(target_->IsEligible());
}

TEST_F(ContextualSearchCueTargetTest, IsEligible_NoBrowserWindow) {
  auto* sync_service = static_cast<syncer::TestSyncService*>(
      SyncServiceFactory::GetForProfile(profile()));
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kHistory, true);

  EXPECT_CALL(*mock_tab_, GetBrowserWindowInterface())
      .WillRepeatedly(testing::Return(nullptr));
  EXPECT_FALSE(target_->IsEligible());
}
#endif

TEST_F(ContextualSearchCueTargetTest, IsPageEligible_LowScoreEdu) {
  auto result = CreateAnnotationResult(CategoryType::kEducation, 60);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest, IsPageEligible_HighScoreEdu) {
  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest, IsPageEligible_EduDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      contextual_tasks::kContextualSearchContextualCuesHandleEdu);

  auto edu_result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_FALSE(target_->IsPageEligible(edu_result, web_contents()));

  auto shopping_result = CreateAnnotationResult(CategoryType::kShopping, 70);
  EXPECT_TRUE(target_->IsPageEligible(shopping_result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest, IsPageEligible_LowScoreShopping) {
  auto result = CreateAnnotationResult(CategoryType::kShopping, 50);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest, IsPageEligible_HighScoreShopping) {
  auto result = CreateAnnotationResult(CategoryType::kShopping, 70);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest, IsPageEligible_ShoppingDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      contextual_tasks::kContextualSearchContextualCuesHandleShopping);

  auto shopping_result = CreateAnnotationResult(CategoryType::kShopping, 70);
  EXPECT_FALSE(target_->IsPageEligible(shopping_result, web_contents()));

  auto edu_result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(edu_result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest,
       IsPageEligible_HighScoreShopping_PdfDiscarded) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kShopping, 80);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest,
       IsPageEligible_HighScoreEdu_PdfNotDiscarded) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest,
       IsPageEligible_HighScoreEduAndShopping_PdfDiscarded) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateMultiAnnotationResult(CategoryType::kEducation, 80,
                                            CategoryType::kShopping, 80);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents()));
}

class ContextualSearchCueTargetDoNotDiscardShoppingPdfsTest
    : public ContextualSearchCueTargetTest {
 public:
  ContextualSearchCueTargetDoNotDiscardShoppingPdfsTest()
      : ContextualSearchCueTargetTest(/*discard_shopping_pdfs=*/false) {}
};

TEST_F(ContextualSearchCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreEdu_Pdf) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreShopping_Pdf) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kShopping, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreEduAndShopping_Pdf) {
  content::WebContentsTester::For(web_contents())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateMultiAnnotationResult(CategoryType::kEducation, 80,
                                            CategoryType::kShopping, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents()));
}

TEST_F(ContextualSearchCueTargetTest, CueActionDataFromResponse) {
  optimization_guide::proto::ContextualCue cue;
  cue.mutable_contextual_search_surface()->set_query("best study tips");

  std::vector<tabs::TabHandle> tabs = {tabs::TabHandle(123)};
  contextual_cueing::CueActionData data =
      target_->CueActionDataFromResponse(cue, tabs);

  ASSERT_TRUE(
      std::holds_alternative<contextual_cueing::ContextualSearchCueActionData>(
          data));
  const auto& cs_data =
      std::get<contextual_cueing::ContextualSearchCueActionData>(data);
  EXPECT_EQ(cs_data.query, "best study tips");
  EXPECT_EQ(cs_data.tabs_to_share, tabs);
}

TEST_F(ContextualSearchCueTargetTest,
       CueActionDataFromResponse_MissingSurface) {
  optimization_guide::proto::ContextualCue cue;
  cue.mutable_gemini_in_chrome_surface()->set_prompt("prompt");

  contextual_cueing::CueActionData data =
      target_->CueActionDataFromResponse(cue, {});

  ASSERT_TRUE(
      std::holds_alternative<contextual_cueing::ContextualSearchCueActionData>(
          data));
  const auto& cs_data =
      std::get<contextual_cueing::ContextualSearchCueActionData>(data);
  EXPECT_TRUE(cs_data.query.empty());
  EXPECT_TRUE(cs_data.tabs_to_share.empty());
}

TEST_F(ContextualSearchCueTargetTest, MetadataAndProperties) {
  EXPECT_EQ(target_->GetType(),
            contextual_cueing::CueTargetType::kContextualSearch);
  EXPECT_EQ(target_->GetSurface(),
            optimization_guide::proto::
                CONTEXTUAL_CUEING_SURFACE_CONTEXTUAL_SEARCH);
  EXPECT_TRUE(target_->RequiresModelExecution());
  EXPECT_FALSE(target_->SupportsEditPrompt());
  EXPECT_TRUE(target_->SupportsIntrusiveness(
      contextual_cueing::CueIntrusiveness::kLoud));
  EXPECT_FALSE(target_->SupportsIntrusiveness(
      contextual_cueing::CueIntrusiveness::kQuiet));
}

}  // namespace

// ---------------------------------------------------------------------------
// Async CheckEligibility tests for ContextualSearchCueTabState
// ---------------------------------------------------------------------------

class ContextualSearchCueTargetAsyncTest
    : public ChromeRenderViewHostTestHarness {
 public:
  ContextualSearchCueTargetAsyncTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_cueing::kContextualCueingV2,
             {{"ContextualCueingV2EduClassifierThreshold", "0.7"},
              {"ContextualCueingV2ShoppingClassifierThreshold", "0.6"}}},
            {contextual_cueing::kContextualCueingV2MultiSource, {}},
            {contextual_tasks::kContextualSearchContextualCuesHandleEdu, {}},
            {contextual_tasks::kContextualSearchContextualCuesHandleShopping,
             {}},
        },
        {});
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                 PageContentAnnotationsServiceFactory::GetInstance(),
                 base::BindRepeating(
                     [](content::BrowserContext* context)
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
                 })}});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    EXPECT_CALL(*mock_tab_, GetProfile())
        .WillRepeatedly(testing::Return(profile()));
    EXPECT_CALL(*mock_tab_, GetContents())
        .WillRepeatedly(testing::Return(web_contents()));
    EXPECT_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(user_data_host_));

    target_ = std::make_unique<ContextualSearchCueTarget>(
        /*optimization_guide_keyed_service=*/nullptr, *mock_tab_);

    cue_tab_state_ = std::make_unique<ContextualSearchCueTabState>(*mock_tab_);
  }

  void TearDown() override {
    target_.reset();
    cue_tab_state_.reset();
    mock_tab_.reset();
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void CallCheckEligibility(base::WeakPtr<content::WebContents> contents,
                            bool* out_eligible) {
    base::RunLoop run_loop;
    target_->CheckEligibility(
        std::move(contents), contextual_cueing::CueIntrusiveness::kLoud,
        base::BindOnce(
            [](bool* out, base::OnceClosure quit, bool eligible,
               contextual_cueing::CueTarget::ContentGenerator) {
              *out = eligible;
              std::move(quit).Run();
            },
            out_eligible, run_loop.QuitClosure()));
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

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<ContextualSearchCueTarget> target_;
  std::unique_ptr<ContextualSearchCueTabState> cue_tab_state_;
};

TEST_F(ContextualSearchCueTargetAsyncTest, CheckEligibility_NullWebContents) {
  bool eligible = true;
  CallCheckEligibility(nullptr, &eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(ContextualSearchCueTargetAsyncTest,
       CheckEligibility_NoAnnotationService) {
  cue_tab_state_->SetAnnotationServiceForTesting(nullptr);
  bool eligible = true;
  CallCheckEligibility(web_contents()->GetWeakPtr(), &eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(ContextualSearchCueTargetAsyncTest, CheckEligibility_CacheHit_Eligible) {
  const GURL url("https://example.com/edu");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());

  bool eligible = false;
  CallCheckEligibility(web_contents()->GetWeakPtr(), &eligible);
  EXPECT_TRUE(eligible);
}

TEST_F(ContextualSearchCueTargetAsyncTest,
       CheckEligibility_CacheHit_Ineligible) {
  const GURL url("https://example.com/low");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateIneligibleResult());

  bool eligible = true;
  CallCheckEligibility(web_contents()->GetWeakPtr(), &eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(ContextualSearchCueTargetAsyncTest,
       CheckEligibility_CacheMiss_AnnotationArrives) {
  const GURL url("https://example.com/pending");
  content::WebContentsTester::For(web_contents())->NavigateAndCommit(url);

  bool eligible = false;
  bool callback_ran = false;
  target_->CheckEligibility(
      web_contents()->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran));

  EXPECT_FALSE(callback_ran);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_ran; }));

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(eligible);
}

}  // namespace contextual_search
