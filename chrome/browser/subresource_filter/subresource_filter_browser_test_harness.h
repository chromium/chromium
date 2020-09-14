// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/subresource_filter/subresource_filter_profile_context.h"
#include "chrome/browser/subresource_filter/test_ruleset_publisher.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/safe_browsing/core/db/util.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "components/subresource_filter/core/common/test_ruleset_creator.h"
#include "components/url_pattern_index/proto/rules.pb.h"
#include "url/gurl.h"

namespace proto = url_pattern_index::proto;

using subresource_filter::testing::ScopedSubresourceFilterConfigurator;
using subresource_filter::testing::TestRulesetPublisher;
using subresource_filter::testing::TestRulesetCreator;
using subresource_filter::testing::TestRulesetPair;

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

class SubresourceFilterContentSettingsManager;
class AdsInterventionManager;
class TestSafeBrowsingDatabaseHelper;

namespace subresource_filter {

class RulesetService;

class SubresourceFilterBrowserTest : public InProcessBrowserTest {
 public:
  SubresourceFilterBrowserTest();
  ~SubresourceFilterBrowserTest() override;

 protected:
  // InProcessBrowserTest:
  void SetUp() override;
  void TearDown() override;
  void SetUpOnMainThread() override;

  virtual std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase();

  GURL GetTestUrl(const std::string& relative_url) const;

  void ConfigureAsPhishingURL(const GURL& url);

  void ConfigureAsSubresourceFilterOnlyURL(const GURL& url);

  void ConfigureURLWithWarning(
      const GURL& url,
      std::vector<safe_browsing::SubresourceFilterType> filter_types);

  content::WebContents* web_contents() const;

  SubresourceFilterContentSettingsManager* settings_manager() const {
    return profile_context_->settings_manager();
  }

  AdsInterventionManager* ads_intervention_manager() {
    return profile_context_->ads_intervention_manager();
  }

  content::RenderFrameHost* FindFrameByName(const std::string& name) const;

  bool WasParsedScriptElementLoaded(content::RenderFrameHost* rfh);

  void ExpectParsedScriptElementLoadedStatusInFrames(
      const std::vector<const char*>& frame_names,
      const std::vector<bool>& expect_loaded);

  void ExpectFramesIncludedInLayout(const std::vector<const char*>& frame_names,
                                    const std::vector<bool>& expect_displayed);

  bool IsDynamicScriptElementLoaded(content::RenderFrameHost* rfh);

  void InsertDynamicFrameWithScript();

  void NavigateFromRendererSide(const GURL& url);

  void NavigateFrame(const char* frame_name, const GURL& url);

  void SetRulesetToDisallowURLsWithPathSuffix(const std::string& suffix);

  void SetRulesetWithRules(const std::vector<proto::UrlRule>& rules);

  // Re-initializes the ruleset_service by opening the ruleset file provided
  // by indexed_ruleset_path and publishing it.
  void OpenAndPublishRuleset(RulesetService* ruleset_service,
                             const base::FilePath& indexed_ruleset_path);

  void ResetConfiguration(Configuration config);

  void ResetConfigurationToEnableOnPhishingSites(
      bool measure_performance = false);

  TestSafeBrowsingDatabaseHelper* database_helper() {
    return database_helper_.get();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  TestRulesetCreator ruleset_creator_;
  ScopedSubresourceFilterConfigurator scoped_configuration_;
  TestRulesetPublisher test_ruleset_publisher_;

  std::unique_ptr<TestSafeBrowsingDatabaseHelper> database_helper_;

  // Owned by the profile.
  SubresourceFilterProfileContext* profile_context_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterBrowserTest);
};

// This class automatically syncs the SubresourceFilter SafeBrowsing list
// without needing a chrome branded build.
class SubresourceFilterListInsertingBrowserTest
    : public SubresourceFilterBrowserTest {
  std::unique_ptr<TestSafeBrowsingDatabaseHelper> CreateTestDatabase() override;
};

}  // namespace subresource_filter

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_SUBRESOURCE_FILTER_BROWSER_TEST_HARNESS_H_
