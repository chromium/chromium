// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_page_action_controller.h"

#include <memory>

#include "base/test/gmock_callback_support.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/views/page_action/test_support/fake_tab_interface.h"
#include "chrome/browser/ui/views/page_action/test_support/mock_page_action_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace indigo {
namespace {

using ::optimization_guide::OptimizationGuideDecision;
using ::testing::_;

struct CreateControllerOptions {
  bool expect_register_optimization_types = true;
};

class IndigoPageActionControllerTest : public testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kIndigo);
  }

  void TearDown() override {
    controller_.reset();
    page_action_controller_.reset();
    tab_interface_.reset();
    mock_optimization_guide_ = nullptr;
    profile_.reset();
  }

  void CreateController(CreateControllerOptions options = {}) {
    CHECK(!controller_);

    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        OptimizationGuideKeyedServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<
              testing::NiceMock<MockOptimizationGuideKeyedService>>();
        }));
    profile_ = builder.Build();

    mock_optimization_guide_ =
        static_cast<testing::NiceMock<MockOptimizationGuideKeyedService>*>(
            OptimizationGuideKeyedServiceFactory::GetForProfile(
                profile_.get()));

    tab_interface_ =
        std::make_unique<page_actions::FakeTabInterface>(profile_.get());
    ON_CALL(*tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(testing::ReturnRef(unowned_user_data_host_));

    page_action_controller_ =
        std::make_unique<page_actions::MockPageActionController>();

    // The service is only created when
    // `optimization_guide::features::IsOptimizationHintsEnabled()` returns
    // true.
    if (mock_optimization_guide_) {
      if (options.expect_register_optimization_types) {
        EXPECT_CALL(*mock_optimization_guide_,
                    RegisterOptimizationTypes(testing::ElementsAre(
                        optimization_guide::proto::OptimizationType::INDIGO)));
      } else {
        EXPECT_CALL(*mock_optimization_guide_,
                    RegisterOptimizationTypes(::testing::Contains(
                        optimization_guide::proto::OptimizationType::INDIGO)))
            .Times(0);
      }
    } else {
      EXPECT_FALSE(options.expect_register_optimization_types)
          << "Cannot expect registration when OptimizationGuideKeyedService "
             "was not created";
    }
    controller_ = std::make_unique<IndigoPageActionController>(
        *tab_interface_, *page_action_controller_);
  }

  void ExpectOptimizationGuideDecision(const GURL& url,
                                       OptimizationGuideDecision decision) {
    EXPECT_CALL(
        *mock_optimization_guide_,
        CanApplyOptimization(
            url, optimization_guide::proto::OptimizationType::INDIGO,
            testing::An<
                optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(base::test::RunOnceCallback<2>(
            decision, optimization_guide::OptimizationMetadata()));
  }

  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<testing::NiceMock<MockOptimizationGuideKeyedService>>
      mock_optimization_guide_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<page_actions::FakeTabInterface> tab_interface_;
  std::unique_ptr<page_actions::MockPageActionController>
      page_action_controller_;
  std::unique_ptr<IndigoPageActionController> controller_;
};

TEST_F(IndigoPageActionControllerTest, ShowsWhenOptimizationGuideReturnsTrue) {
  CreateController();

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));

  GURL url("https://example.com");
  ExpectOptimizationGuideDecision(url, OptimizationGuideDecision::kTrue);

  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

TEST_F(IndigoPageActionControllerTest, HidesWhenOptimizationGuideReturnsFalse) {
  CreateController();

  // First, simulate a navigation where the decision is true so it gets shown.
  GURL url1("https://example.com");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  // Now expect Hide when navigating to a page where the decision is false.
  EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));

  GURL url2("https://example2.com");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);

  auto navigation2 = content::NavigationSimulator::CreateBrowserInitiated(
      url2, tab_interface_->GetContents());
  navigation2->Commit();
}

TEST_F(IndigoPageActionControllerTest, UpdatesOnSameDocumentNavigation) {
  CreateController();

  // First, simulate a navigation where the decision is true so it gets shown.
  GURL url1("https://example.com/page1");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  // Now expect Hide when performing a same-document navigation to a page where
  // the decision is false.
  EXPECT_CALL(*page_action_controller_, Hide(kActionIndigo));

  GURL url2("https://example.com/page2");
  ExpectOptimizationGuideDecision(url2, OptimizationGuideDecision::kFalse);

  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      url2, tab_interface_->GetContents()->GetPrimaryMainFrame());
  navigation2->CommitSameDocument();
}

TEST_F(IndigoPageActionControllerTest, IgnoresFragmentOnlyNavigation) {
  CreateController();

  GURL url1("https://example.com/page");
  ExpectOptimizationGuideDecision(url1, OptimizationGuideDecision::kTrue);

  EXPECT_CALL(*page_action_controller_, Show(kActionIndigo));
  EXPECT_CALL(*page_action_controller_, ShowSuggestionChip(kActionIndigo, _));

  auto navigation1 = content::NavigationSimulator::CreateBrowserInitiated(
      url1, tab_interface_->GetContents());
  navigation1->Commit();

  testing::Mock::VerifyAndClearExpectations(mock_optimization_guide_);
  testing::Mock::VerifyAndClearExpectations(page_action_controller_.get());

  // Now perform a same-document navigation that only changes the fragment.
  // We expect NO calls to mock_optimization_guide_ or page_action_controller_.
  EXPECT_CALL(
      *mock_optimization_guide_,
      CanApplyOptimization(
          _, _,
          testing::An<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);
  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
  EXPECT_CALL(*page_action_controller_, Hide(_)).Times(0);

  GURL url2("https://example.com/page#fragment");
  auto navigation2 = content::NavigationSimulator::CreateRendererInitiated(
      url2, tab_interface_->GetContents()->GetPrimaryMainFrame());
  navigation2->CommitSameDocument();
}

TEST_F(IndigoPageActionControllerTest,
       HidesWhenOptimizationHintsFeatureIsDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      optimization_guide::features::kOptimizationHints);

  CreateController({.expect_register_optimization_types = false});

  EXPECT_CALL(*page_action_controller_, Show(_)).Times(0);
  GURL url("https://example.com");
  auto navigation = content::NavigationSimulator::CreateBrowserInitiated(
      url, tab_interface_->GetContents());
  navigation->Commit();
}

}  // namespace
}  // namespace indigo
