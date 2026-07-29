// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/suggestions/glic_cue_target.h"

#include <memory>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/suggestions/glic_cue_tab_state.h"
#include "chrome/browser/glic/test_support/mock_glic_keyed_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/page_content_annotations/core/test_page_content_annotations_service.h"
#include "components/pdf/common/constants.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace glic {
namespace {

using ::page_content_annotations::Category;
using ::page_content_annotations::CategoryType;
using ::page_content_annotations::PageContentAnnotationsResult;

class GlicCueTargetTest : public testing::Test {
 public:
  explicit GlicCueTargetTest(bool discard_shopping_pdfs = true) {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_cueing::kContextualCueingV2,
             {{"ContextualCueingV2EduClassifierThreshold", "0.7"},
              {"ContextualCueingV2ShoppingClassifierThreshold", "0.6"},
              {"ContextualCueingV2DiscardShoppingPdfs",
               discard_shopping_pdfs ? "true" : "false"}}},
        },
        {});
  }

  void SetUp() override {
    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();

    profile_ = testing_profile_manager->CreateTestingProfile(
        "TestProfile", std::move(testing_factories));

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);

    GlicEnabling::SetBypassEnablementChecksForTesting(true);

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);

    // We don't strictly need these to be fully functional because
    // IsPageEligible doesn't use them, but the constructor requires them.
    mock_glic_keyed_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, IdentityManagerFactory::GetForProfile(profile_),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_, nullptr, nullptr);

    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    EXPECT_CALL(*mock_tab_, GetProfile())
        .WillRepeatedly(testing::Return(profile_));

    target_ = std::make_unique<GlicCueTarget>(
        *mock_glic_keyed_service_,
        /*optimization_guide_keyed_service=*/nullptr, *mock_tab_);
  }

  void TearDown() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    target_.reset();
    mock_tab_.reset();
    mock_glic_keyed_service_.reset();
    web_contents_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
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
  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<TestingProfile> profile_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  GlicProfileManager glic_profile_manager_;

  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_keyed_service_;

  std::unique_ptr<GlicCueTarget> target_;
};

TEST_F(GlicCueTargetTest, IsPageEligible_LowScoreEdu) {
  auto result = CreateAnnotationResult(CategoryType::kEducation, 60);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreEdu) {
  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_LowScoreShopping) {
  auto result = CreateAnnotationResult(CategoryType::kShopping, 50);
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreShopping) {
  auto result = CreateAnnotationResult(CategoryType::kShopping, 70);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreShopping_PdfDiscarded) {
  // Simulate a PDF.
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kShopping, 80);
  // Although it's high score shopping, we discard shopping PDFs.
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreEdu_PdfNotDiscarded) {
  // Simulate a PDF.
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  // High score edu PDFs are still eligible.
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetTest, IsPageEligible_HighScoreEduAndShopping_PdfDiscarded) {
  // Simulate a PDF.
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateMultiAnnotationResult(CategoryType::kEducation, 80,
                                            CategoryType::kShopping, 80);
  // When kDiscardShoppingPdfs is true, having shopping present disqualifies it.
  EXPECT_FALSE(target_->IsPageEligible(result, web_contents_.get()));
}

class GlicCueTargetDoNotDiscardShoppingPdfsTest : public GlicCueTargetTest {
 public:
  GlicCueTargetDoNotDiscardShoppingPdfsTest()
      : GlicCueTargetTest(/*discard_shopping_pdfs=*/false) {}
};

TEST_F(GlicCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreEdu_Pdf) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kEducation, 80);
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreShopping_Pdf) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateAnnotationResult(CategoryType::kShopping, 80);
  // When kDiscardShoppingPdfs is false, shopping PDFs are eligible.
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

TEST_F(GlicCueTargetDoNotDiscardShoppingPdfsTest,
       IsPageEligible_HighScoreEduAndShopping_Pdf) {
  content::WebContentsTester::For(web_contents_.get())
      ->SetMainFrameMimeType(pdf::kPDFMimeType);

  auto result = CreateMultiAnnotationResult(CategoryType::kEducation, 80,
                                            CategoryType::kShopping, 80);
  // When kDiscardShoppingPdfs is false, having both high scores is eligible.
  EXPECT_TRUE(target_->IsPageEligible(result, web_contents_.get()));
}

}  // namespace

// ---------------------------------------------------------------------------
// Async CheckEligibility tests for GlicCueTabState
// ---------------------------------------------------------------------------

class GlicCueTargetAsyncTest : public testing::Test {
 public:
  GlicCueTargetAsyncTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {contextual_cueing::kContextualCueingV2,
             {{"ContextualCueingV2EduClassifierThreshold", "0.7"},
              {"ContextualCueingV2ShoppingClassifierThreshold", "0.6"}}},
            {contextual_cueing::kContextualCueingV2MultiSource, {}},
        },
        {});
  }

  void SetUp() override {
    TestingProfileManager* testing_profile_manager =
        TestingBrowserProcess::GetGlobal()->SetUpGlobalFeaturesForTesting(
            /*profile_manager=*/true);
    TestingProfile::TestingFactories testing_factories =
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactories();
    testing_factories.emplace_back(
        PageContentAnnotationsServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return page_content_annotations::TestPageContentAnnotationsService::
              Create(
                  /*optimization_guide_model_provider=*/nullptr,
                  /*history_service=*/nullptr);
        }));
    testing_factories.emplace_back(
        HistoryServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext* context)
                -> std::unique_ptr<KeyedService> { return nullptr; }));
    profile_ = testing_profile_manager->CreateTestingProfile(
        "TestProfile", std::move(testing_factories));
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_);
    GlicEnabling::SetBypassEnablementChecksForTesting(true);
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(profile_, nullptr);
    mock_glic_keyed_service_ = std::make_unique<MockGlicKeyedService>(
        profile_, IdentityManagerFactory::GetForProfile(profile_),
        TestingBrowserProcess::GetGlobal()->profile_manager(),
        &glic_profile_manager_, nullptr, nullptr);
    mock_tab_ = std::make_unique<tabs::MockTabInterface>();
    EXPECT_CALL(*mock_tab_, GetProfile())
        .WillRepeatedly(testing::Return(profile_));
    EXPECT_CALL(*mock_tab_, GetContents())
        .WillRepeatedly(testing::Return(web_contents_.get()));
    EXPECT_CALL(*mock_tab_, GetUnownedUserDataHost())
        .WillRepeatedly(testing::ReturnRef(user_data_host_));

    target_ = std::make_unique<GlicCueTarget>(
        *mock_glic_keyed_service_,
        /*optimization_guide_keyed_service=*/nullptr, *mock_tab_);

    cue_tab_state_ = std::make_unique<GlicCueTabState>(*mock_tab_);
  }

  void TearDown() override {
    GlicEnabling::SetBypassEnablementChecksForTesting(false);
    target_.reset();
    cue_tab_state_.reset();
    mock_tab_.reset();
    mock_glic_keyed_service_.reset();
    web_contents_.reset();
    identity_test_env_adaptor_.reset();
    profile_ = nullptr;
    TestingBrowserProcess::GetGlobal()->TearDownGlobalFeaturesForTesting();
  }

  void CallCheckEligibility(base::WeakPtr<content::WebContents> web_contents,
                            bool* out_eligible) {
    base::RunLoop run_loop;
    target_->CheckEligibility(
        std::move(web_contents), contextual_cueing::CueIntrusiveness::kLoud,
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
    if (web_contents_->GetController().GetLastCommittedEntry()) {
      visit.nav_entry_timestamp = web_contents_->GetController()
                                      .GetLastCommittedEntry()
                                      ->GetTimestamp();
    }
    return visit;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_{
      content::BrowserTaskEnvironment::TimeSource::MOCK_TIME};

  raw_ptr<TestingProfile> profile_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  GlicProfileManager glic_profile_manager_;
  ui::UnownedUserDataHost user_data_host_;
  std::unique_ptr<tabs::MockTabInterface> mock_tab_;
  std::unique_ptr<MockGlicKeyedService> mock_glic_keyed_service_;
  std::unique_ptr<GlicCueTarget> target_;
  std::unique_ptr<GlicCueTabState> cue_tab_state_;
};

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_NullWebContents) {
  bool eligible = true;
  CallCheckEligibility(nullptr, &eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_NoAnnotationService) {
  cue_tab_state_->SetAnnotationServiceForTesting(nullptr);
  bool eligible = true;
  CallCheckEligibility(web_contents_->GetWeakPtr(), &eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_CacheHit_Eligible) {
  const GURL url("https://example.com/edu");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url);

  // Pre-populate the cache with an eligible annotation.
  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());

  bool eligible = false;
  CallCheckEligibility(web_contents_->GetWeakPtr(), &eligible);
  EXPECT_TRUE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_CacheHit_Ineligible) {
  const GURL url("https://example.com/low");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url);

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateIneligibleResult());

  bool eligible = true;
  CallCheckEligibility(web_contents_->GetWeakPtr(), &eligible);
  EXPECT_FALSE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_CacheMiss_AnnotationArrives) {
  const GURL url("https://example.com/pending");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url);

  bool eligible = false;
  bool callback_ran = false;
  target_->CheckEligibility(
      web_contents_->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran));

  // Callback should not have fired synchronously.
  EXPECT_FALSE(callback_ran);

  // Simulate annotation arriving.
  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_ran; }));

  EXPECT_TRUE(callback_ran);
  EXPECT_TRUE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_CacheMiss_Timeout) {
  const GURL url("https://example.com/timeout");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url);

  bool eligible = true;
  bool callback_ran = false;
  target_->CheckEligibility(
      web_contents_->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran));

  EXPECT_FALSE(callback_ran);

  // Fast-forward past the 3-second default timeout.
  task_environment_.FastForwardBy(base::Seconds(4));

  EXPECT_TRUE(callback_ran);
  EXPECT_FALSE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_NewCheckCancelsPending) {
  const GURL url1("https://example.com/first");
  const GURL url2("https://different.com/second");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url1);

  bool eligible1 = true;
  bool callback1_ran = false;
  target_->CheckEligibility(
      web_contents_->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible1, &callback1_ran));

  // Navigate to a second URL. DidFinishNavigation cancels callback1.
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url2);

  // First callback should be cancelled (fired with false) by
  // DidFinishNavigation. Wait for the cancellation task to run clean.
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback1_ran; }));
  EXPECT_TRUE(callback1_ran);
  EXPECT_FALSE(eligible1);

  // Now safely issue a new check for url2.
  bool eligible2 = false;
  bool callback2_ran = false;
  target_->CheckEligibility(
      web_contents_->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible2, &callback2_ran));

  // Second check is now pending. Deliver annotation for url2.
  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url2),
                                         CreateEligibleResult());
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback2_ran; }));

  EXPECT_TRUE(callback2_ran);
  EXPECT_TRUE(eligible2);
}

TEST_F(GlicCueTargetAsyncTest, CheckEligibility_TabStateDestroyedDuringWait) {
  const GURL url("https://example.com/destroyed");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url);

  bool eligible = true;
  bool callback_ran = false;
  target_->CheckEligibility(
      web_contents_->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran));

  // Destroy the tab state while the check is pending, as TabFeatures does
  // when the tab goes away. GlicCueTabState's destructor will fire the
  // pending callback with false.
  cue_tab_state_.reset();
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_ran; }));

  EXPECT_TRUE(callback_ran);
  EXPECT_FALSE(eligible);
}

TEST_F(GlicCueTargetAsyncTest, DestructorFiresPendingCallback) {
  const GURL url("https://example.com/dtor");
  content::WebContentsTester::For(web_contents_.get())->NavigateAndCommit(url);

  bool eligible = true;
  bool callback_ran = false;
  target_->CheckEligibility(
      web_contents_->GetWeakPtr(), contextual_cueing::CueIntrusiveness::kLoud,
      base::BindOnce(
          [](bool* out_eligible, bool* out_ran, bool eligible,
             contextual_cueing::CueTarget::ContentGenerator) {
            *out_eligible = eligible;
            *out_ran = true;
          },
          &eligible, &callback_ran));

  // Destroy target while check is pending.
  // When the annotation arrives later, GlicCueTabState will see target is gone
  // and resolve with false.
  target_.reset();

  cue_tab_state_->OnPageContentAnnotated(CreateVisit(url),
                                         CreateEligibleResult());
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_ran; }));

  EXPECT_TRUE(callback_ran);
  EXPECT_FALSE(eligible);
}

}  // namespace glic
