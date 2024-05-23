// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/tpcd/metadata/devtools_observer.h"

#include "base/files/file_path.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/subresource_filter/subresource_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/prefs/pref_service.h"
#include "components/subresource_filter/core/common/test_ruleset_utils.h"
#include "components/tpcd/metadata/browser/parser.h"
#include "content/public/browser/cookie_access_details.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace tpcd::metadata {
namespace {

using ::chrome_test_utils::GetActiveWebContents;

}  // namespace

class TpcdMetadataDevtoolsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest,
      public content::TestDevToolsProtocolClient {
 public:
  explicit TpcdMetadataDevtoolsObserverBrowserTest(
      bool enable_metadata_feature = true)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    enabled_features_.push_back(
        {content_settings::features::kTrackingProtection3pcd, {}});
    enabled_features_.push_back(
        {network::features::kSkipTpcdMitigationsForAds,
         {{"SkipTpcdMitigationsForAdsMetadata", "true"}}});
    if (enable_metadata_feature) {
      enabled_features_.push_back({net::features::kTpcdMetadataGrants, {}});
    } else {
      disabled_features_.push_back(net::features::kTpcdMetadataGrants);
    }

    feature_list_.InitWithFeaturesAndParameters(enabled_features_,
                                                disabled_features_);
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
    host_resolver()->AddRule("c.test", "127.0.0.1");

    // Set up HTTPS server with image for cookie.
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    ASSERT_TRUE(https_server_.Start());

    // Open and reset DevTools.
    AttachToWebContents(GetActiveWebContents(this));
    SendCommandSync("Audits.enable");
    ClearNotifications();

    // Initialize mock 3PCD metadata component.
    const std::string first_party_pattern_spec = "[*.]a.test";
    const std::string third_party_pattern_spec_1 = "[*.]b.test";
    const std::string third_party_pattern_spec_2 = "c.test";

    Metadata metadata;
    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, third_party_pattern_spec_1, first_party_pattern_spec,
        Parser::kSource1pDt, /*dtrp=*/50u);
    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, third_party_pattern_spec_2, first_party_pattern_spec,
        Parser::kSource3pDt, /*dtrp=*/50u, /*dtrp_override=*/20u);
    tpcd::metadata::Parser::GetInstance()->ParseMetadata(
        metadata.SerializeAsString());

    // Initialize devtools WebContentsObserver.
    devtools_observer_ = TpcdMetadataDevtoolsObserver::FromWebContents(
        GetActiveWebContents(this));

    // These rules apply an ad-tagging param to cookies marked with the `isad=1`
    // param value.
    SetRulesetWithRules(
        {subresource_filter::testing::CreateSuffixRule("isad=1")});
  }

  void TearDownOnMainThread() override {
    DetachProtocolClient();
    devtools_observer_ = nullptr;
  }

 protected:
  void AddCookieAccess(const std::string& first_party_site,
                       const std::string& third_party_site,
                       bool is_ad_tagged) {
    ASSERT_TRUE(NavigateToSetCookie(
        GetActiveWebContents(this), &https_server_, third_party_site,
        /*is_secure_cookie_set=*/true, is_ad_tagged));

    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(this),
        embedded_test_server()->GetURL(first_party_site, "/title1.html")));

    std::string relative_url = "/favicon/icon.png";
    if (is_ad_tagged) {
      relative_url += "?isad=1";
    }
    CreateImageAndWaitForCookieAccess(
        GetActiveWebContents(this),
        https_server_.GetURL(third_party_site, relative_url));
  }

  void WaitForIssueAndCheck(const std::vector<std::string>& sites,
                            uint32_t opt_out_percentage,
                            bool is_opt_out_top_level) {
    auto is_metadata_issue = [](const base::Value::Dict& params) {
      const std::string* issue_code =
          params.FindStringByDottedPath("issue.code");
      return issue_code && *issue_code == "CookieDeprecationMetadataIssue";
    };

    // Wait for notification of a Metadata Issue.
    base::Value::Dict params = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(is_metadata_issue));
    const std::string* issue_code = params.FindStringByDottedPath("issue.code");
    ASSERT_TRUE(issue_code);
    ASSERT_EQ(*issue_code, "CookieDeprecationMetadataIssue");

    base::Value::Dict* metadata_issue_details = params.FindDictByDottedPath(
        "issue.details.cookieDeprecationMetadataIssueDetails");
    ASSERT_TRUE(metadata_issue_details);

    // Verify the reported allowed sites match the expected sites.
    std::vector<std::string> allowed_sites;
    base::Value::List* allowed_sites_list =
        metadata_issue_details->FindList("allowedSites");
    if (allowed_sites_list) {
      for (const auto& val : *allowed_sites_list) {
        allowed_sites.push_back(val.GetString());
      }
    }
    EXPECT_THAT(allowed_sites, testing::ElementsAreArray(sites));

    // Verify the reported DTRP values against the expected values.
    EXPECT_EQ(
        static_cast<uint32_t>(
            metadata_issue_details->FindInt("optOutPercentage").value_or(0)),
        opt_out_percentage);
    EXPECT_EQ(
        metadata_issue_details->FindBool("isOptOutTopLevel").value_or(false),
        is_opt_out_top_level);
    EXPECT_THAT(
        *metadata_issue_details,
        base::test::DictionaryHasValue("operation", base::Value("ReadCookie")));

    // Clear existing notifications so subsequent calls don't fail by checking
    // `sites` against old notifications.
    ClearNotifications();
  }

  void CheckNoAddedIssue() {
    ReportDummyIssue();

    WaitForIssueAndCheck({"dummy.test"}, 0u, false);
  }

 private:
  void ReportDummyIssue() {
    auto details = blink::mojom::InspectorIssueDetails::New();

    auto metadata_issue_details =
        blink::mojom::CookieDeprecationMetadataIssueDetails::New();
    metadata_issue_details->allowed_sites.push_back("dummy.test");
    metadata_issue_details->operation =
        blink::mojom::CookieOperation::kReadCookie;
    details->cookie_deprecation_metadata_issue_details =
        std::move(metadata_issue_details);

    GetActiveWebContents(this)->GetPrimaryMainFrame()->ReportInspectorIssue(
        blink::mojom::InspectorIssueInfo::New(
            blink::mojom::InspectorIssueCode::kCookieDeprecationMetadataIssue,
            std::move(details)));
  }

  base::test::ScopedFeatureList feature_list_;
  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
  net::EmbeddedTestServer https_server_;
  raw_ptr<TpcdMetadataDevtoolsObserver> devtools_observer_ = nullptr;
};

// TODO(https://crbug.com/341211478): Flaky.
IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       DISABLED_EmitsDevtoolsIssues) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForIssueAndCheck({"b.test"}, 50u, true);

  AddCookieAccess("a.test", "c.test", /*is_ad_tagged=*/false);
  WaitForIssueAndCheck({"c.test"}, 20u, false);
}

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       DoesNotEmitDevtoolsIssueForSiteNotInAllowlist) {
  AddCookieAccess("b.test", "a.test", /*is_ad_tagged=*/false);

  CheckNoAddedIssue();
}

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       DoesNotEmitDevtoolsIssueWithBlockedCookiesSetting) {
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kBlockAll3pcToggleEnabled,
                                               true);

  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);

  CheckNoAddedIssue();
}

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       DoesNotEmitDevtoolsIssueForAdTaggedCookie) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/true);

  CheckNoAddedIssue();
}

class TpcdMetadataDevtoolsObserverDisabledBrowserTest
    : public TpcdMetadataDevtoolsObserverBrowserTest {
 public:
  TpcdMetadataDevtoolsObserverDisabledBrowserTest()
      : TpcdMetadataDevtoolsObserverBrowserTest(
            /*enable_metadata_feature=*/false) {}
};

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverDisabledBrowserTest,
                       DoesNotEmitDevtoolsIssueWithMissingFeature) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);

  CheckNoAddedIssue();
}

}  // namespace tpcd::metadata
