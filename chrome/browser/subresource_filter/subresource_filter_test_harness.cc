// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_service.h"
#include "chrome/browser/subresource_filter/chrome_subresource_filter_client.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/browser/subresource_filter/subresource_filter_test_harness.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "components/subresource_filter/content/browser/ruleset_service.h"
#include "components/subresource_filter/content/browser/subresource_filter_observer_test_utils.h"
#include "components/subresource_filter/core/common/activation_decision.h"
#include "components/subresource_filter/core/common/activation_list.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

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

  // Set up safe browsing service with the fake database manager.
  //
  // TODO(csharrison): This is a bit ugly. See if the instructions in
  // test_safe_browsing_service.h can be adapted to be used in unit tests.
  safe_browsing::TestSafeBrowsingServiceFactory sb_service_factory;
  fake_safe_browsing_database_ = new FakeSafeBrowsingDatabaseManager();
  sb_service_factory.SetTestDatabaseManager(fake_safe_browsing_database_.get());
  safe_browsing::SafeBrowsingService::RegisterFactory(&sb_service_factory);
  auto* safe_browsing_service = sb_service_factory.CreateSafeBrowsingService();
  safe_browsing::SafeBrowsingService::RegisterFactory(nullptr);
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(
      safe_browsing_service);
  g_browser_process->safe_browsing_service()->Initialize();

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
  auto ruleset_service = std::make_unique<subresource_filter::RulesetService>(
      &pref_service_, base::ThreadTaskRunnerHandle::Get(),
      ruleset_service_dir_.GetPath(), base::ThreadTaskRunnerHandle::Get());
  TestingBrowserProcess::GetGlobal()->SetRulesetService(
      std::move(ruleset_service));

  // Publish the test ruleset.
  subresource_filter::testing::TestRulesetCreator ruleset_creator;
  subresource_filter::testing::TestRulesetPair test_ruleset_pair;
  ruleset_creator.CreateRulesetToDisallowURLsWithPathSuffix("disallowed.html",
                                                            &test_ruleset_pair);
  subresource_filter::testing::TestRulesetPublisher test_ruleset_publisher;
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));

  // Set up the tab helpers.
  InfoBarService::CreateForWebContents(web_contents());
  TabSpecificContentSettings::CreateForWebContents(web_contents());
  ChromeSubresourceFilterClient::CreateForWebContents(web_contents());

  base::RunLoop().RunUntilIdle();
}

void SubresourceFilterTestHarness::TearDown() {
  fake_safe_browsing_database_ = nullptr;
  TestingBrowserProcess::GetGlobal()->safe_browsing_service()->ShutDown();

  // Must explicitly set these to null and pump the run loop to ensure that
  // all cleanup related to these classes actually happens.
  TestingBrowserProcess::GetGlobal()->SetRulesetService(nullptr);
  TestingBrowserProcess::GetGlobal()->SetSafeBrowsingService(nullptr);

  base::RunLoop().RunUntilIdle();

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
  fake_safe_browsing_database_->AddBlacklistedUrl(
      url, safe_browsing::SB_THREAT_TYPE_SUBRESOURCE_FILTER);
}

ChromeSubresourceFilterClient* SubresourceFilterTestHarness::GetClient() {
  return ChromeSubresourceFilterClient::FromWebContents(web_contents());
}

void SubresourceFilterTestHarness::RemoveURLFromBlacklist(const GURL& url) {
  fake_safe_browsing_database_->RemoveBlacklistedUrl(url);
}

SubresourceFilterContentSettingsManager*
SubresourceFilterTestHarness::GetSettingsManager() {
  return SubresourceFilterProfileContextFactory::GetForProfile(
             static_cast<Profile*>(profile()))
      ->settings_manager();
}
