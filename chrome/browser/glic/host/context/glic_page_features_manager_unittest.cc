// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_page_features_manager.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/navigation_simulator.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

class GlicPageFeaturesManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  GlicPageFeaturesManagerTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

namespace glic {

class TestGlicPageFeaturesManager : public GlicPageFeaturesManager {
 public:
  explicit TestGlicPageFeaturesManager(tabs::TabInterface* tab)
      : GlicPageFeaturesManager(tab) {}

  using GlicPageFeaturesManager::OnCheckResult;

  bool IsTimerRunning() const { return check_timer_.IsRunning(); }
};

TEST_F(GlicPageFeaturesManagerTest, CheckCachingAndRetries) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSummarizeVideoSuggestion);

  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents()));
  ON_CALL(mock_tab, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  auto manager = std::make_unique<TestGlicPageFeaturesManager>(&mock_tab);

  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.youtube.com/watch?v=123"));

  // Check should be scheduled.
  EXPECT_TRUE(manager->IsTimerRunning());

  // Fast forward to the first run.
  task_environment()->FastForwardBy(GlicPageFeaturesManager::kCheckDelay);
  EXPECT_FALSE(manager->IsTimerRunning());

  // Simulate first failure, it should retry.
  manager->OnCheckResult(base::Value(false));
  EXPECT_TRUE(manager->IsTimerRunning());

  // Fast forward to second run.
  task_environment()->FastForwardBy(GlicPageFeaturesManager::kCheckDelay);
  EXPECT_FALSE(manager->IsTimerRunning());

  // Second failure, no more retries.
  manager->OnCheckResult(base::Value(false));
  EXPECT_FALSE(manager->IsTimerRunning());
  EXPECT_TRUE(manager->GetFeatures().empty());

  // Success on a later navigation.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.youtube.com/watch?v=456"));
  task_environment()->FastForwardBy(GlicPageFeaturesManager::kCheckDelay);
  manager->OnCheckResult(base::Value(true));

  const std::vector<mojom::LightweightPageFeature>& features =
      manager->GetFeatures();
  ASSERT_EQ(1u, features.size());
  EXPECT_EQ(mojom::LightweightPageFeature::kYtAskButtonPresent, features[0]);
}

TEST_F(GlicPageFeaturesManagerTest, WillDiscardContents) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSummarizeVideoSuggestion);

  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents()));
  ON_CALL(mock_tab, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  auto manager = std::make_unique<TestGlicPageFeaturesManager>(&mock_tab);

  // Navigate to YouTube and succeed.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.youtube.com/watch?v=123"));
  task_environment()->FastForwardBy(GlicPageFeaturesManager::kCheckDelay);
  manager->OnCheckResult(base::Value(true));
  EXPECT_EQ(1u, manager->GetFeatures().size());

  // Prepare new WebContents (discarding simulation).
  std::unique_ptr<content::WebContents> new_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(profile()));
  // Navigate new_contents to YouTube before discarding.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      new_contents.get(), GURL("https://www.youtube.com/watch?v=456"));

  // Simulate discard.
  manager->WillDiscardContents(&mock_tab, web_contents(), new_contents.get());

  // State should be cleared.
  EXPECT_TRUE(manager->GetFeatures().empty());
  // The new check should have been scheduled because new_contents was already
  // on YouTube.
  EXPECT_TRUE(manager->IsTimerRunning());

  // Fast forward.
  task_environment()->FastForwardBy(GlicPageFeaturesManager::kCheckDelay);
}

TEST_F(GlicPageFeaturesManagerTest, HistogramLogging) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGlicSummarizeVideoSuggestion);

  tabs::MockTabInterface mock_tab;
  ui::UnownedUserDataHost user_data_host;
  ON_CALL(mock_tab, GetContents())
      .WillByDefault(testing::Return(web_contents()));
  ON_CALL(mock_tab, GetUnownedUserDataHost())
      .WillByDefault(testing::ReturnRef(user_data_host));

  base::HistogramTester histogram_tester;
  auto manager = std::make_unique<TestGlicPageFeaturesManager>(&mock_tab);

  // Navigate to YouTube.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.youtube.com/watch?v=123"));

  // Simulate first failure.
  manager->OnCheckResult(base::Value(false));

  // Simulate second failure.
  manager->OnCheckResult(base::Value(false));
  histogram_tester.ExpectBucketCount(
      "Glic.YoutubeSummarizeVideoZSS.Events",
      GlicPageFeaturesManager::YoutubeSummarizeVideoZSS::
          kButtonNotFoundAfterAllChecks,
      1);

  // Navigate again and succeed on first check.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.youtube.com/watch?v=456"));
  manager->OnCheckResult(base::Value(true));
  histogram_tester.ExpectBucketCount(
      "Glic.YoutubeSummarizeVideoZSS.Events",
      GlicPageFeaturesManager::YoutubeSummarizeVideoZSS::
          kButtonFoundOnFirstCheck,
      1);

  // Navigate again and succeed on second check.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("https://www.youtube.com/watch?v=789"));
  manager->OnCheckResult(base::Value(false));  // First fail
  manager->OnCheckResult(base::Value(true));   // Second success
  histogram_tester.ExpectBucketCount(
      "Glic.YoutubeSummarizeVideoZSS.Events",
      GlicPageFeaturesManager::YoutubeSummarizeVideoZSS::
          kButtonFoundOnSecondCheck,
      1);
}

}  // namespace glic
