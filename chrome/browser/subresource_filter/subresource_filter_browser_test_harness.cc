// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/test_safe_browsing_database_helper.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/blocked_content/safe_browsing_triggered_popup_blocker.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "components/safe_browsing/core/browser/db/v4_test_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/subresource_filter/content/browser/content_subresource_filter_throttle_manager.h"
#include "components/subresource_filter/content/browser/subresource_filter_profile_context.h"
#include "components/subresource_filter/content/browser/test_ruleset_publisher.h"
#include "components/subresource_filter/content/shared/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/verified_ruleset_dealer.h"
#include "components/subresource_filter/core/common/common_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace subresource_filter {

// static
const char SubresourceFilterBrowserTest::kSubresourceLoadsTotalForPage[];
const char SubresourceFilterBrowserTest::kSubresourceLoadsEvaluatedForPage[];
const char SubresourceFilterBrowserTest::kSubresourceLoadsMatchedRulesForPage[];
const char SubresourceFilterBrowserTest::kSubresourceLoadsDisallowedForPage[];
const char SubresourceFilterBrowserTest::kEvaluationTotalWallDurationForPage[];
const char SubresourceFilterBrowserTest::kEvaluationTotalCPUDurationForPage[];
const char SubresourceFilterBrowserTest::kEvaluationWallDuration[];
const char SubresourceFilterBrowserTest::kEvaluationCPUDuration[];
const char SubresourceFilterBrowserTest::kActivationDecision[];
const char SubresourceFilterBrowserTest::kActivationListHistogram[];
const char SubresourceFilterBrowserTest::kPageLoadActivationStateHistogram[];
const char
    SubresourceFilterBrowserTest::kPageLoadActivationStateDidInheritHistogram[];
const char SubresourceFilterBrowserTest::kSubresourceFilterActionsHistogram[];

MockSubresourceFilterObserver::MockSubresourceFilterObserver(
    content::WebContents* web_contents) {
  scoped_observation_.Observe(
      SubresourceFilterObserverManager::FromWebContents(web_contents));
}

MockSubresourceFilterObserver::~MockSubresourceFilterObserver() = default;

// ================= SubresourceFilterSharedBrowserTest =======================

SubresourceFilterSharedBrowserTest::SubresourceFilterSharedBrowserTest() =
    default;

SubresourceFilterSharedBrowserTest::~SubresourceFilterSharedBrowserTest() =
    default;

void SubresourceFilterSharedBrowserTest::SetUpOnMainThread() {
  embedded_test_server()->ServeFilesFromSourceDirectory("components/test/data");
  host_resolver()->AddSimulatedFailure("host-with-dns-lookup-failure");

  host_resolver()->AddRule("*", "127.0.0.1");
  content::SetupCrossSiteRedirector(embedded_test_server());

  // This does not start the embedded test server in order to allow derived
  // classes to perform additional setup.
}

GURL SubresourceFilterSharedBrowserTest::GetTestUrl(
    const std::string& relative_url) const {
  return embedded_test_server()->base_url().Resolve(relative_url);
}

content::WebContents* SubresourceFilterSharedBrowserTest::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

content::RenderFrameHost* SubresourceFilterSharedBrowserTest::FindFrameByName(
    const std::string& name) {
  return content::FrameMatchingPredicate(
      web_contents()->GetPrimaryPage(),
      base::BindRepeating(&content::FrameMatchesName, name));
}

bool SubresourceFilterSharedBrowserTest::WasParsedScriptElementLoaded(
    content::RenderFrameHost* rfh) {
  CHECK(rfh);
  return content::EvalJs(rfh, "!!document.scriptExecuted").ExtractBool();
}

void SubresourceFilterSharedBrowserTest::
    ExpectParsedScriptElementLoadedStatusInFrames(
        const std::vector<const char*>& frame_names,
        const std::vector<bool>& expect_loaded) {
  ASSERT_EQ(expect_loaded.size(), frame_names.size());
  for (size_t i = 0; i < frame_names.size(); ++i) {
    SCOPED_TRACE(frame_names[i]);
    content::RenderFrameHost* frame = FindFrameByName(frame_names[i]);
    ASSERT_TRUE(frame);
    ASSERT_EQ(expect_loaded[i], WasParsedScriptElementLoaded(frame));
  }
}

void SubresourceFilterSharedBrowserTest::ExpectFramesIncludedInLayout(
    const std::vector<const char*>& frame_names,
    const std::vector<bool>& expect_displayed) {
  const char kScript[] = "document.getElementsByName(\"%s\")[0].clientWidth;";

  ASSERT_EQ(expect_displayed.size(), frame_names.size());
  for (size_t i = 0; i < frame_names.size(); ++i) {
    SCOPED_TRACE(frame_names[i]);
    int client_width =
        content::EvalJs(web_contents()->GetPrimaryMainFrame(),
                        base::StringPrintf(kScript, frame_names[i]))
            .ExtractInt();
    EXPECT_EQ(expect_displayed[i], !!client_width) << client_width;
  }
}

void SubresourceFilterSharedBrowserTest::NavigateFrame(const char* frame_name,
                                                       const GURL& url) {
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  ASSERT_TRUE(content::ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      base::StringPrintf("document.getElementsByName(\"%s\")[0].src = \"%s\";",
                         frame_name, url.spec().c_str())));
  navigation_observer.Wait();
}

// ======================= SubresourceFilterBrowserTest =======================

SubresourceFilterBrowserTest::SubresourceFilterBrowserTest() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kAdTagging},
      /*disabled_features=*/{features::kHttpsUpgrades});
}

SubresourceFilterBrowserTest::~SubresourceFilterBrowserTest() = default;

bool SubresourceFilterBrowserTest::AdsBlockedInContentSettings(
    content::RenderFrameHost* frame_host) {
  auto* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(frame_host);

  return content_settings->IsContentBlocked(ContentSettingsType::ADS);
}

void SubresourceFilterBrowserTest::SetUp() {
  database_helper_ = CreateTestDatabase();
  SubresourceFilterSharedBrowserTest::SetUp();
}

void SubresourceFilterBrowserTest::TearDown() {
  SubresourceFilterSharedBrowserTest::TearDown();
  // Unregister test factories after PlatformBrowserTest::TearDown
  // (which destructs SafeBrowsingService).
  database_helper_.reset();
}

void SubresourceFilterBrowserTest::SetUpOnMainThread() {
  SubresourceFilterSharedBrowserTest::SetUpOnMainThread();
  // Add content/test/data for cross_site_iframe_factory.html
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
  embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

  ASSERT_TRUE(embedded_test_server()->Start());

  auto* web_contents = chrome_test_utils::GetActiveWebContents(this);
  profile_context_ = SubresourceFilterProfileContextFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents->GetBrowserContext()));
}

std::unique_ptr<TestSafeBrowsingDatabaseHelper>
SubresourceFilterBrowserTest::CreateTestDatabase() {
  return std::make_unique<TestSafeBrowsingDatabaseHelper>();
}

void SubresourceFilterBrowserTest::ConfigureAsPhishingURL(const GURL& url) {
  safe_browsing::ThreatMetadata metadata;
  database_helper_->AddFullHashToDbAndFullHashCache(
      url, safe_browsing::GetUrlSocEngId(), metadata);
}

void SubresourceFilterBrowserTest::ConfigureAsSubresourceFilterOnlyURL(
    const GURL& url) {
  safe_browsing::ThreatMetadata metadata;
  database_helper_->AddFullHashToDbAndFullHashCache(
      url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
}

void SubresourceFilterBrowserTest::ConfigureURLWithWarning(
    const GURL& url,
    std::vector<safe_browsing::SubresourceFilterType> filter_types) {
  safe_browsing::ThreatMetadata metadata;

  for (auto type : filter_types) {
    metadata.subresource_filter_match[type] =
        safe_browsing::SubresourceFilterLevel::WARN;
  }
  database_helper_->AddFullHashToDbAndFullHashCache(
      url, safe_browsing::GetUrlSubresourceFilterId(), metadata);
}

bool SubresourceFilterBrowserTest::IsDynamicScriptElementLoaded(
    content::RenderFrameHost* rfh) {
  CHECK(rfh);
  return content::EvalJs(rfh, "insertScriptElementAndReportSuccess()")
      .ExtractBool();
}

void SubresourceFilterBrowserTest::InsertDynamicFrameWithScript() {
  EXPECT_EQ(true, content::EvalJs(web_contents()->GetPrimaryMainFrame(),
                                  "insertFrameWithScriptAndNotify()"));
}

void SubresourceFilterBrowserTest::NavigateFromRendererSide(const GURL& url) {
  content::TestNavigationObserver navigation_observer(web_contents(), 1);
  ASSERT_TRUE(content::ExecJs(
      web_contents()->GetPrimaryMainFrame(),
      base::StringPrintf("window.location = \"%s\";", url.spec().c_str())));
  navigation_observer.Wait();
}

void SubresourceFilterBrowserTest::SetRulesetToDisallowURLsWithPathSuffix(
    const std::string& suffix) {
  TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetToDisallowURLsWithPathSuffix(
      suffix, &test_ruleset_pair);

  TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->subresource_filter_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

void SubresourceFilterBrowserTest::SetRulesetToDisallowURLsWithSubstrings(
    std::vector<std::string_view> substrings) {
  TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetToDisallowURLWithSubstrings(
      std::move(substrings), &test_ruleset_pair);

  TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->subresource_filter_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}

void SubresourceFilterBrowserTest::SetRulesetWithRules(
    const std::vector<proto::UrlRule>& rules) {
  TestRulesetPair test_ruleset_pair;
  ruleset_creator_.CreateRulesetWithRules(rules, &test_ruleset_pair);

  TestRulesetPublisher test_ruleset_publisher(
      g_browser_process->subresource_filter_ruleset_service());
  ASSERT_NO_FATAL_FAILURE(
      test_ruleset_publisher.SetRuleset(test_ruleset_pair.unindexed));
}


void SubresourceFilterBrowserTest::OpenAndPublishRuleset(
    RulesetService* ruleset_service,
    const base::FilePath& indexed_ruleset_path) {
  RulesetFilePtr index_file(nullptr, base::OnTaskRunnerDeleter(nullptr));
  base::RunLoop open_loop;
  auto open_callback = base::BindLambdaForTesting(
      [&index_file, &open_loop](RulesetFilePtr result) {
        index_file = std::move(result);
        open_loop.Quit();
      });
  IndexedRulesetVersion version =
      ruleset_service->GetMostRecentlyIndexedVersion();
  ruleset_service->GetRulesetDealer()->TryOpenAndSetRulesetFile(
      indexed_ruleset_path, version.checksum, std::move(open_callback));
  open_loop.Run();
  ASSERT_TRUE(index_file->IsValid());
  ruleset_service->OnRulesetSet(std::move(index_file));
}

void SubresourceFilterBrowserTest::ResetConfiguration(Configuration config) {
  scoped_configuration_.ResetConfiguration(std::move(config));
}

void SubresourceFilterBrowserTest::ResetConfigurationToEnableOnPhishingSites(
    bool measure_performance) {
  Configuration config = Configuration::MakePresetForLiveRunOnPhishingSites();
  config.activation_options.performance_measurement_rate =
      measure_performance ? 1.0 : 0.0;
  ResetConfiguration(std::move(config));
}

std::unique_ptr<TestSafeBrowsingDatabaseHelper>
SubresourceFilterListInsertingBrowserTest::CreateTestDatabase() {
  std::vector<safe_browsing::ListIdentifier> list_ids = {
      safe_browsing::GetUrlSubresourceFilterId()};
  return std::make_unique<TestSafeBrowsingDatabaseHelper>(
      std::make_unique<safe_browsing::TestV4GetHashProtocolManagerFactory>(),
      std::move(list_ids));
}

SubresourceFilterPrerenderingBrowserTest::
    SubresourceFilterPrerenderingBrowserTest()
    : prerender_helper_(base::BindRepeating(
          &SubresourceFilterPrerenderingBrowserTest::web_contents,
          base::Unretained(this))) {}

SubresourceFilterPrerenderingBrowserTest::
    ~SubresourceFilterPrerenderingBrowserTest() = default;

void SubresourceFilterPrerenderingBrowserTest::SetUp() {
  prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
  SubresourceFilterListInsertingBrowserTest::SetUp();
}

}  // namespace subresource_filter
