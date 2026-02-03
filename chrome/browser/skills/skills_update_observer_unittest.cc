// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/skills/skills_update_observer.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/hints/optimization_guide_decision.h"
#include "components/optimization_guide/core/hints/optimization_metadata.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/skills/features.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::testing::_;
using ::testing::A;
using ::testing::Return;
using ::testing::WithArgs;

class TestTabInterface : public tabs::MockTabInterface {
 public:
  TestTabInterface() = default;
  ~TestTabInterface() override = default;

  ui::UnownedUserDataHost& GetUnownedUserDataHost() override {
    return user_data_host_;
  }
  const ui::UnownedUserDataHost& GetUnownedUserDataHost() const override {
    return user_data_host_;
  }

 private:
  ui::UnownedUserDataHost user_data_host_;
};

}  // namespace

class SkillsUpdateObserverTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    feature_list_.InitWithFeatures(
        {optimization_guide::features::kOptimizationHints,
         features::kSkillsEnabled},
        {});
    // The mock Optimization Guide service requires kOptimizationHints to be
    // enabled, to work properly.
    mock_optimization_guide_keyed_service_ = static_cast<
        MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile(),
                base::BindRepeating([](content::BrowserContext* context)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockOptimizationGuideKeyedService>();
                })));
    tab_interface_ = std::make_unique<TestTabInterface>();
    ON_CALL(*tab_interface_, GetContents).WillByDefault(Return(web_contents()));
    observer_ =
        std::make_unique<skills::SkillsUpdateObserver>(*(tab_interface_.get()));
  }

  void TearDown() override {
    observer_.reset();
    tab_interface_.reset();
    mock_optimization_guide_keyed_service_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Simulates a navigation to the given URL.
  void SimulateNavigation(const GURL& url) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                               url);
  }

  // Sets up an expectation for the mock Optimization Guide service.
  void ExpectOptimizationGuideDecision(
      const GURL& url,
      optimization_guide::OptimizationGuideDecision decision,
      const std::optional<skills::proto::SkillsList>& skills_list) {
    EXPECT_CALL(*mock_optimization_guide_keyed_service_,
                CanApplyOptimization(
                    _, optimization_guide::proto::OptimizationType::SKILLS,
                    A<optimization_guide::OptimizationGuideDecisionCallback>()))
        .WillOnce(WithArgs<2>(
            [decision,
             skills_list](optimization_guide::OptimizationGuideDecisionCallback
                              callback) {
              optimization_guide::OptimizationMetadata metadata;
              if (skills_list.has_value()) {
                skills::proto::SkillsList skills_list_copy =
                    skills_list.value();
                optimization_guide::proto::Any any_metadata;
                any_metadata.set_type_url(
                    "type.googleapis.com/skills.proto.SkillsList");
                skills_list_copy.SerializeToString(
                    any_metadata.mutable_value());
                metadata.set_any_metadata(any_metadata);
              }
              std::move(callback).Run(decision, metadata);
            }));
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<tabs::MockTabInterface> tab_interface_;
  std::unique_ptr<skills::SkillsUpdateObserver> observer_;
  raw_ptr<MockOptimizationGuideKeyedService>
      mock_optimization_guide_keyed_service_;
};

// Test that the Optimization Guide is not called if the feature is disabled.
TEST_F(SkillsUpdateObserverTest, FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures({}, {features::kSkillsEnabled});
  GURL url("https://www.example.com");

  EXPECT_CALL(*mock_optimization_guide_keyed_service_,
              CanApplyOptimization(
                  url, optimization_guide::proto::OptimizationType::SKILLS,
                  A<optimization_guide::OptimizationGuideDecisionCallback>()))
      .Times(0);

  SimulateNavigation(url);
  EXPECT_EQ(observer_->contextual_skills(), nullptr);
}

// Test that the contextual skills are not updated when the Optimization Guide
// decision is kFalse.
TEST_F(SkillsUpdateObserverTest, OnOptimizationGuideDecision_IsFalse) {
  GURL url("https://www.example.com");
  ExpectOptimizationGuideDecision(
      url, optimization_guide::OptimizationGuideDecision::kFalse, std::nullopt);

  SimulateNavigation(url);

  // Verify that the SkillsList is not updated.
  EXPECT_EQ(observer_->contextual_skills(), nullptr);
}

// Test the case where the Optimization Guide decision is kTrue but no metadata
// is provided.
TEST_F(SkillsUpdateObserverTest, OnOptimizationGuideDecision_NoMetadata) {
  GURL url("https://www.example.com");
  ExpectOptimizationGuideDecision(
      url, optimization_guide::OptimizationGuideDecision::kTrue, std::nullopt);

  SimulateNavigation(url);

  EXPECT_EQ(observer_->contextual_skills(), nullptr);
}

// Test that the contextual skills are updated when the Optimization Guide
// decision is kTrue.
TEST_F(SkillsUpdateObserverTest, OnOptimizationGuideDecision_IsTrue) {
  GURL url("https://www.example.com");
  skills::proto::SkillsList skills_list;
  skills::proto::Skill* skill_1 = skills_list.add_skills();
  skill_1->set_id("test_skill_1");
  skill_1->set_name("Test Skill 1");
  skill_1->set_icon("test_icon_1");
  skills::proto::Skill* skill_2 = skills_list.add_skills();
  skill_2->set_id("test_skill_2");
  skill_2->set_name("Test Skill 2");
  skill_2->set_icon("test_icon_2");
  ExpectOptimizationGuideDecision(
      url, optimization_guide::OptimizationGuideDecision::kTrue, skills_list);

  SimulateNavigation(url);

  // Verify that the SkillsList is updated.
  const skills::proto::SkillsList* actual_skills =
      observer_->contextual_skills();
  EXPECT_EQ(actual_skills->skills_size(), 2);
  const skills::proto::Skill& actual_skill_1 = actual_skills->skills(0);
  EXPECT_EQ(actual_skill_1.id(), "test_skill_1");
  EXPECT_EQ(actual_skill_1.name(), "Test Skill 1");
  EXPECT_EQ(actual_skill_1.icon(), "test_icon_1");
  const skills::proto::Skill& actual_skill_2 = actual_skills->skills(1);
  EXPECT_EQ(actual_skill_2.id(), "test_skill_2");
  EXPECT_EQ(actual_skill_2.name(), "Test Skill 2");
  EXPECT_EQ(actual_skill_2.icon(), "test_icon_2");
}

// Test that the contextual skills are updated on successive navigations.
TEST_F(SkillsUpdateObserverTest, NavigationUpdatesSkills) {
  GURL url1("https://www.example1.com");
  GURL url2("https://www.example2.com");

  skills::proto::SkillsList skills1;
  skills1.add_skills()->set_name("Skill 1");
  ExpectOptimizationGuideDecision(
      url1, optimization_guide::OptimizationGuideDecision::kTrue, skills1);

  SimulateNavigation(url1);
  const skills::proto::SkillsList* result1 = observer_->contextual_skills();
  EXPECT_EQ(result1->skills_size(), 1);
  EXPECT_EQ(result1->skills(0).name(), "Skill 1");

  skills::proto::SkillsList skills2;
  skills2.add_skills()->set_name("Skill 2");
  ExpectOptimizationGuideDecision(
      url2, optimization_guide::OptimizationGuideDecision::kTrue, skills2);

  SimulateNavigation(url2);
  const skills::proto::SkillsList* result2 = observer_->contextual_skills();
  EXPECT_EQ(result2->skills_size(), 1);
  EXPECT_EQ(result2->skills(0).name(), "Skill 2");
}
