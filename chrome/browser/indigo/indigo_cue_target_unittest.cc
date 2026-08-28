// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_cue_target.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/contextual_cueing/cue_target.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/indigo/resources/grit/indigo_strings.h"
#include "chrome/browser/skills/skills_service_factory.h"
#include "chrome/browser/ui/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/page_action/test_support/mock_page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/page_content_annotations/core/page_content_annotations_common.h"
#include "components/skills/mocks/mock_skills_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace indigo {
namespace {

class IndigoCueTargetTest : public testing::Test {
 protected:
  IndigoCueTargetTest() {
    feature_list_.InitWithFeatures(
        {features::kIndigo, features::kIndigoContextualCueingV2}, {});
  }
  ~IndigoCueTargetTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        skills::SkillsServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<skills::MockSkillsService>>();
        }));
    profile_ = builder.Build();

    tab_interface_ =
        std::make_unique<page_actions::FakeTabInterface>(profile_.get());
    page_action_controller_ = std::make_unique<
        testing::NiceMock<page_actions::MockPageActionController>>();

    controller_ = std::make_unique<IndigoPageActionController>(
        *tab_interface_, *page_action_controller_);

    mock_skills_service_ =
        static_cast<testing::NiceMock<skills::MockSkillsService>*>(
            skills::SkillsServiceFactory::GetForProfile(profile_.get()));

    service_ = IndigoServiceFactory::GetForProfile(profile_.get());
    ASSERT_TRUE(service_);
    service_->SetLocalEligibilityForTesting(LocalEligibility::kEligible);

    cue_target_ = std::make_unique<IndigoCueTarget>(*service_, *tab_interface_);
  }

  void TearDown() override {
    cue_target_.reset();
    controller_.reset();
    page_action_controller_.reset();
    tab_interface_.reset();
    service_ = nullptr;
    mock_skills_service_ = nullptr;
    profile_.reset();
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<page_actions::FakeTabInterface> tab_interface_;
  std::unique_ptr<testing::NiceMock<page_actions::MockPageActionController>>
      page_action_controller_;
  std::unique_ptr<IndigoPageActionController> controller_;
  raw_ptr<testing::NiceMock<skills::MockSkillsService>> mock_skills_service_;
  raw_ptr<IndigoService> service_;
  std::unique_ptr<IndigoCueTarget> cue_target_;
};

TEST_F(IndigoCueTargetTest, BasicProperties) {
  EXPECT_EQ(cue_target_->GetType(), contextual_cueing::CueTargetType::kIndigo);
  EXPECT_TRUE(cue_target_->IsEligible());
  EXPECT_FALSE(cue_target_->IsPageEligible(
      page_content_annotations::PageContentAnnotationsResult::
          CreateCategoryResults({}),
      nullptr));
  EXPECT_EQ(cue_target_->GetSurface(),
            optimization_guide::proto::CONTEXTUAL_CUEING_SURFACE_UNSPECIFIED);
}

TEST_F(IndigoCueTargetTest, OnClickInvokesAnchoredMessage) {
  base::UserActionTester user_action_tester;
  cue_target_->OnClick(std::monostate{});
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Indigo.PageAction.AnchoredMessage.Click"));
}

TEST_F(IndigoCueTargetTest, CheckEligibility_NullWebContents) {
  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_target_->CheckEligibility(nullptr,
                                contextual_cueing::CueIntrusiveness::kLoud,
                                future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
}

TEST_F(IndigoCueTargetTest, CheckEligibility_LocallyIneligible) {
  service_->SetLocalEligibilityForTesting(LocalEligibility::kNotSignedIn);
  EXPECT_FALSE(cue_target_->IsEligible());

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_target_->CheckEligibility(tab_interface_->GetContents()->GetWeakPtr(),
                                contextual_cueing::CueIntrusiveness::kLoud,
                                future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
}

TEST_F(IndigoCueTargetTest, CheckEligibility_EligibleGeneratesContent) {
  base::test::ScopedCommandLine scoped_command_line;
  scoped_command_line.GetProcessCommandLine()->AppendSwitch(kForceIndigoSwitch);
  EXPECT_CALL(*mock_skills_service_, RefreshDiscoverySkills()).Times(1);
  base::HistogramTester histogram_tester;

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_target_->CheckEligibility(tab_interface_->GetContents()->GetWeakPtr(),
                                contextual_cueing::CueIntrusiveness::kLoud,
                                future.GetCallback());
  EXPECT_TRUE(future.Get<0>());

  contextual_cueing::CueTarget::ContentGenerator generator =
      std::get<1>(future.Take());
  ASSERT_TRUE(generator);

  base::test::TestFuture<
      std::optional<optimization_guide::proto::ContextualCue>>
      cue_future;
  std::move(generator).Run(cue_future.GetCallback());

  auto cue_opt = cue_future.Take();
  ASSERT_TRUE(cue_opt.has_value());
  EXPECT_EQ(
      cue_opt->suggested_cuj(),
      contextual_cueing::GetName(contextual_cueing::CueTargetType::kIndigo));
  EXPECT_EQ(cue_opt->anchored_message_cue().action_text(),
            base::UTF16ToUTF8(
                l10n_util::GetStringUTF16(IDS_INDIGO_ENTRYPOINT_CHIP_TEXT)));
  EXPECT_EQ(cue_opt->anchored_message_cue().anchored_message_text(),
            base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                IDS_INDIGO_ENTRYPOINT_ANCHORED_MESSAGE_TEXT)));

  histogram_tester.ExpectUniqueSample("Indigo.PageAction.TriggerSource",
                                      IndigoTriggerSource::kForced, 1);
}

TEST_F(IndigoCueTargetTest, CheckEligibility_Ineligible) {
  IndigoPageActionController::TestApi(controller_.get())
      .SetOptimizationGuideDecisionForTesting(
          optimization_guide::OptimizationGuideDecision::kFalse);

  base::test::TestFuture<bool, contextual_cueing::CueTarget::ContentGenerator>
      future;
  cue_target_->CheckEligibility(tab_interface_->GetContents()->GetWeakPtr(),
                                contextual_cueing::CueIntrusiveness::kLoud,
                                future.GetCallback());
  EXPECT_FALSE(future.Get<0>());
}

}  // namespace
}  // namespace indigo
