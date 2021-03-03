// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/subresource_filter/subresource_filter_test_harness.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/safe_browsing/core/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

constexpr char const SubresourceFilterTestHarness::kDefaultAllowedSuffix[];
constexpr char const SubresourceFilterTestHarness::kDefaultDisallowedSuffix[];
constexpr char const SubresourceFilterTestHarness::kDefaultDisallowedUrl[];

SubresourceFilterTestHarness::SubresourceFilterTestHarness() = default;
SubresourceFilterTestHarness::~SubresourceFilterTestHarness() = default;

// ChromeRenderViewHostTestHarness:
void SubresourceFilterTestHarness::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Ensure correct features.
  scoped_configuration_.ResetConfiguration(subresource_filter::Configuration(
      subresource_filter::mojom::ActivationLevel::kEnabled,
      subresource_filter::ActivationScope::ACTIVATION_LIST,
      subresource_filter::ActivationList::SUBRESOURCE_FILTER));

  NavigateAndCommit(GURL("https://example.first"));

  // Set up the ruleset service.
  ASSERT_TRUE(ruleset_service_dir_.CreateUniqueTempDir());
  subresource_filter::IndexedRulesetVersion::RegisterPrefs(
      pref_service_.registry());
  // TODO(csharrison): having separated blocking and background task runners
  // for |ContentRulesetService| and |RulesetService| would be a good idea, but
  // external unit tests code implicitly uses knowledge that blocking and
  // background task runners are initiazlied from
  // |base::ThreadTaskRunnerHandle::Get()|:
  // 1. |TestRulesetPublisher| uses this knowledge in |SetRuleset| method. It
  //    is waiting for the ruleset published callback.
  // 2. Navigation simulator uses this knowledge. It knows that
  //    |AsyncDocumentSubresourceFilter| posts core initialization tasks on
  //    blocking task runner and this it is the current thread task runner.
  ruleset_service_ = std::make_unique<subresource_filter::RulesetService>(
      &pref_service_, base::ThreadTaskRunnerHandle::Get(),
      ruleset_service_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get());

  // Publish the test ruleset.
  subresource_filter::testing::TestRulesetCreator ruleset_creator;
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator.CreateRulesetWithRules(
      {subresource_filter::testing::CreateSuffixRule(kDefaultDisallowedSuffix),
       subresource_filter::testing::CreateAllowlistSuffixRule(
           kDefaultAllowedSuffix)},
      &test_ruleset_pair);
  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher(
      ruleset_service_.get());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));

  subresource_filter::VerifiedRulesetDealer::Handle* dealer =
      ruleset_service_.get()->GetRulesetDealer();
  auto client =
      std::make_unique<subresource_filter::TestSubresourceFilterClient>(
          web_contents());
  client_ = client.get();
  client_->CreateSafeBrowsingDatabaseManager();
  subresource_filter::ContentSubresourceFilterThrottleManager::
      CreateForWebContents(web_contents(), std::move(client), dealer);

  base::RunLoop().RunUntilIdle();
}

void SubresourceFilterTestHarness::TearDown() {
  ruleset_service_.reset();

  ChromeRenderViewHostTestHarness::TearDown();
}

// Will return nullptr if the navigation fails.
content::RenderFrameHost*
SubresourceFilterTestHarness::SimulateNavigateAndCommit(
    const GURL& url,
    content::RenderFrameHost* rfh) {
  auto simulator =
      content::NavigationSimulator::CreateRendererInitiated(url, rfh);
  simulator->Commit();
  return simulator->GetLastThrottleCheckResult().action() ==
                 content::NavigationThrottle::PROCEED
             ? simulator->GetFinalRenderFrameHost()
             : nullptr;
}

// Returns the frame host the navigation commit in, or nullptr if it did not
// succeed.
content::RenderFrameHost*
SubresourceFilterTestHarness::CreateAndNavigateDisallowedSubframe(
    content::RenderFrameHost* parent) {
  auto* subframe =
      content::RenderFrameHostTester::For(parent)->AppendChild("subframe");
  return SimulateNavigateAndCommit(GURL(kDefaultDisallowedUrl), subframe);
}

void SubresourceFilterTestHarness::ConfigureAsSubresourceFilterOnlyURL(
    const GURL& url) {
  fake_safe_browsing_database()->AddBlocklistedUrl(
      url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
}

void SubresourceFilterTestHarness::RemoveURLFromBlocklist(const GURL& url) {
  fake_safe_browsing_database()->RemoveBlocklistedUrl(url);
}

subresource_filter::SubresourceFilterContentSettingsManager*
SubresourceFilterTestHarness::GetSettingsManager() {
  return client_->profile_context()->settings_manager();
}

void SubresourceFilterTestHarness::TagSubframeAsAd(
    content::RenderFrameHost* render_frame_host) {
  subresource_filter::ContentSubresourceFilterThrottleManager::FromWebContents(
      web_contents())
      ->SetFrameAsAdSubframeForTesting(render_frame_host);
}
