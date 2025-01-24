// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/tpcd/metadata/devtools_observer.h"

#include "base/files/file_path.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace tpcd::metadata {
namespace {

using ::chrome_test_utils::GetActiveWebContents;

class URLCookieAccessObserver : public content::WebContentsObserver {
 public:
  URLCookieAccessObserver(content::WebContents* web_contents,
                          GURL url,
                          content::CookieAccessDetails::Type access_type);

  void Wait();

 private:
  // WebContentsObserver overrides
  void OnCookiesAccessed(content::RenderFrameHost* render_frame_host,
                         const content::CookieAccessDetails& details) override;
  void OnCookiesAccessed(content::NavigationHandle* navigation_handle,
                         const content::CookieAccessDetails& details) override;

  GURL url_;
  content::CookieAccessDetails::Type access_type_;
  base::RunLoop run_loop_;
};

URLCookieAccessObserver::URLCookieAccessObserver(
    content::WebContents* web_contents,
    GURL url,
    content::CookieAccessDetails::Type access_type)
    : WebContentsObserver(web_contents),
      url_(std::move(url)),
      access_type_(access_type) {}

void URLCookieAccessObserver::Wait() {
  run_loop_.Run();
}

void URLCookieAccessObserver::OnCookiesAccessed(
    content::RenderFrameHost* render_frame_host,
    const content::CookieAccessDetails& details) {
  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

void URLCookieAccessObserver::OnCookiesAccessed(
    content::NavigationHandle* navigation_handle,
    const content::CookieAccessDetails& details) {
  if (details.type == access_type_ && details.url == url_) {
    run_loop_.Quit();
  }
}

bool NavigateToSetCookie(content::WebContents* web_contents,
                         const net::EmbeddedTestServer* server,
                         std::string_view host,
                         bool is_secure_cookie_set,
                         bool is_ad_tagged) {
  std::string relative_url = "/set-cookie?name=value";
  if (is_secure_cookie_set) {
    relative_url += ";Secure;SameSite=None";
  }
  if (is_ad_tagged) {
    relative_url += "&isad=1";
  }
  const auto url = server->GetURL(host, relative_url);

  URLCookieAccessObserver observer(web_contents, url,
                                   content::CookieAccessDetails::Type::kChange);
  bool success = content::NavigateToURL(web_contents, url);
  if (success) {
    observer.Wait();
  }
  return success;
}

void CreateImageAndWaitForCookieAccess(content::WebContents* web_contents,
                                       const GURL& image_url) {
  URLCookieAccessObserver observer(web_contents, image_url,
                                   content::CookieAccessDetails::Type::kRead);
  ASSERT_TRUE(content::ExecJs(web_contents,
                              content::JsReplace(
                                  R"(
    let img = document.createElement('img');
    img.src = $1;
    document.body.appendChild(img);)",
                                  image_url),
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  // The image must cause a cookie access, or else this will hang.
  observer.Wait();
}

}  // namespace

class TpcdMetadataDevtoolsObserverBrowserTest
    : public subresource_filter::SubresourceFilterBrowserTest {
 public:
  explicit TpcdMetadataDevtoolsObserverBrowserTest(
      bool enable_tracking_protection = true,
      bool enable_metadata_feature = true,
      bool enable_staged_control = true)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    enabled_features_.push_back(
        {network::features::kSkipTpcdMitigationsForAds,
         {{"SkipTpcdMitigationsForAdsMetadata", "true"}}});
    // Since Tracking Protection is always enabled via the field trial config
    // for browser tests, we need to manually disable it.
    if (enable_tracking_protection) {
      enabled_features_.push_back(
          {content_settings::features::kTrackingProtection3pcd, {}});
    } else {
      disabled_features_.push_back(
          content_settings::features::kTrackingProtection3pcd);
    }
    if (enable_metadata_feature) {
      enabled_features_.push_back({net::features::kTpcdMetadataGrants, {}});
    } else {
      disabled_features_.push_back(net::features::kTpcdMetadataGrants);
    }

    enable_staged_control_ = enable_staged_control;
    if (!enable_staged_control) {
      disabled_features_.push_back(net::features::kTpcdMetadataStageControl);
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
    web_contents_devtools_client.AttachToWebContents(
        GetActiveWebContents(this));
    web_contents_devtools_client.SendCommandSync("Audits.enable");
    web_contents_devtools_client.SendCommandSync("Network.enable");
    web_contents_devtools_client.ClearNotifications();

    // Initialize mock 3PCD metadata component.
    const std::string first_party_pattern_spec = "[*.]a.test";
    const std::string third_party_pattern_spec_1 = "[*.]b.test";
    const std::string third_party_pattern_spec_2 = "c.test";

    Metadata metadata;
    // If staged control is enabled, DTRP values must be set to 0 to avoid
    // flakiness if the entry is dropped. If staged control is disabled, the
    // values can be tested.
    std::optional<uint32_t> dtrp =
        enable_staged_control_ ? std::nullopt : std::make_optional(50u);
    std::optional<uint32_t> dtrp_override =
        enable_staged_control_ ? std::nullopt : std::make_optional(20u);
    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, third_party_pattern_spec_1, first_party_pattern_spec,
        Parser::kSource1pDt, dtrp);
    tpcd::metadata::helpers::AddEntryToMetadata(
        metadata, third_party_pattern_spec_2, first_party_pattern_spec,
        Parser::kSource3pDt, dtrp, dtrp_override);
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
    web_contents_devtools_client.DetachProtocolClient();
    frame_devtools_client.DetachProtocolClient();
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

  void WaitForMetadataIssueAndCheck(const std::vector<std::string>& sites,
                                    uint32_t opt_out_percentage,
                                    bool is_opt_out_top_level) {
    auto is_metadata_issue = [](const base::Value::Dict& params) {
      const std::string* issue_code =
          params.FindStringByDottedPath("issue.code");
      return issue_code && *issue_code == "CookieDeprecationMetadataIssue";
    };

    // Wait for notification of a Metadata Issue.
    base::Value::Dict params =
        web_contents_devtools_client.WaitForMatchingNotification(
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
    web_contents_devtools_client.ClearNotifications();
  }

  void WaitForCookieIssueAndCheck(std::string_view third_party_site,
                                  std::string_view warning,
                                  std::string_view exclusion) {
    CHECK(warning.empty() || exclusion.empty())
        << "inclusion reason and exclusion reason should not co-exist";
    auto is_cookie_issue = [](const base::Value::Dict& params) {
      const std::string* issue_code =
          params.FindStringByDottedPath("issue.code");
      return issue_code && *issue_code == "CookieIssue";
    };

    // Wait for notification of a Cookie Issue.
    base::Value::Dict params =
        web_contents_devtools_client.WaitForMatchingNotification(
            "Audits.issueAdded", base::BindRepeating(is_cookie_issue));

    std::string_view reason_name =
        warning.empty() ? "cookieExclusionReasons" : "cookieWarningReasons";
    std::string_view reason_value = warning.empty() ? exclusion : warning;
    std::string partial_expected =
        content::JsReplace(R"({
            "cookie": {
               "domain": $1,
               "name": "name",
               "path": "/"
            },
            $2: [ $3 ],
            "operation": "ReadCookie",
         })",
                           third_party_site, reason_name, reason_value);

    // Find relevant fields from cookieIssueDetails
    ASSERT_THAT(params.FindDictByDottedPath("issue.details.cookieIssueDetails"),
                testing::Pointee(base::test::DictionaryHasValues(
                    base::test::ParseJsonDict(partial_expected))));

    web_contents_devtools_client.ClearNotifications();
  }

  void CheckNoAddedIssue() {
    ReportDummyIssue();

    WaitForMetadataIssueAndCheck({"dummy.test"}, 0u, false);
  }

  void SendSetCookieControls(bool enable_third_party_cookie_restriction,
                             bool disable_third_party_cookie_metadata,
                             bool disable_third_party_cookie_heuristics) {
    base::Value::Dict command_params;
    command_params.Set("enableThirdPartyCookieRestriction",
                       enable_third_party_cookie_restriction);
    command_params.Set("disableThirdPartyCookieMetadata",
                       disable_third_party_cookie_metadata);
    command_params.Set("disableThirdPartyCookieHeuristics",
                       disable_third_party_cookie_heuristics);
    web_contents_devtools_client.SendCommandSync("Network.setCookieControls",
                                                 std::move(command_params));
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(GetActiveWebContents(this)->GetPrimaryMainFrame(), 0);
  }

  void NavigateToPageWith3pIFrame(std::string_view host) {
    frame_devtools_client.DetachProtocolClient();
    GURL main_url(https_server().GetURL(host, "/iframe.html"));

    ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(this), main_url));
    EXPECT_TRUE(
        NavigateIframeToURL(GetActiveWebContents(this), "test",
                            https_server().GetURL("b.test", "/blank.html")));

    frame_devtools_client.AttachToFrameTreeHost(GetFrame());
    frame_devtools_client.SendCommandSync("Network.enable");
  }

  std::string SetCookieFromJS(content::RenderFrameHost* render_frame_host,
                              std::string cookie) {
    content::EvalJsResult result = content::EvalJs(
        render_frame_host,
        "document.cookie = '" + cookie + "; SameSite=None; Secure'",
        content::EXECUTE_SCRIPT_NO_USER_GESTURE);

    return result.ExtractString();
  }

  std::string ReadCookiesFromJS(content::RenderFrameHost* render_frame_host) {
    std::string res = content::EvalJs(render_frame_host, "document.cookie",
                                      content::EXECUTE_SCRIPT_NO_USER_GESTURE)
                          .ExtractString();

    return res;
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

  content::TestDevToolsProtocolClient web_contents_devtools_client;
  content::TestDevToolsProtocolClient frame_devtools_client;

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

  bool enable_staged_control_ = true;
  base::test::ScopedFeatureList feature_list_;
  std::vector<base::test::FeatureRefAndParams> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
  net::EmbeddedTestServer https_server_;
  raw_ptr<TpcdMetadataDevtoolsObserver> devtools_observer_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       EmitsDevtoolsIssues) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForMetadataIssueAndCheck({"b.test"}, 0u, true);

  AddCookieAccess("a.test", "c.test", /*is_ad_tagged=*/false);
  WaitForMetadataIssueAndCheck({"c.test"}, 0u, false);
}

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       DoesNotEmitMetadataIssueWhenDevToolsDisableMetadata) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForMetadataIssueAndCheck({"b.test"}, 0u, true);

  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/true,
                        /*disable_third_party_cookie_heuristics=*/false);

  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  CheckNoAddedIssue();
}

class TpcdMetadataDevtoolsObserverTrackingProtectionDisabledBrowserTest
    : public TpcdMetadataDevtoolsObserverBrowserTest {
 public:
  TpcdMetadataDevtoolsObserverTrackingProtectionDisabledBrowserTest()
      : TpcdMetadataDevtoolsObserverBrowserTest(
            /*enable_tracking_protection=*/false) {}
};

IN_PROC_BROWSER_TEST_F(
    TpcdMetadataDevtoolsObserverTrackingProtectionDisabledBrowserTest,
    EmitCookieIssueWhenDevToolsBlockTPC) {
  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/false,
                        /*disable_third_party_cookie_heuristics=*/false);

  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForCookieIssueAndCheck("b.test", {"WarnDeprecationTrialMetadata"}, {});
}

IN_PROC_BROWSER_TEST_F(
    TpcdMetadataDevtoolsObserverTrackingProtectionDisabledBrowserTest,
    EmitMetadataIssueWhenDevToolsBlockTPC) {
  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/false,
                        /*disable_third_party_cookie_heuristics=*/false);

  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForMetadataIssueAndCheck({"b.test"}, 0u, true);
}

IN_PROC_BROWSER_TEST_F(
    TpcdMetadataDevtoolsObserverTrackingProtectionDisabledBrowserTest,
    EmitCookieIssueWhenDevToolsDisableMetadata) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForCookieIssueAndCheck("b.test", {"WarnThirdPartyPhaseout"}, {});

  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/true,
                        /*disable_third_party_cookie_heuristics=*/false);

  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  // Since the cookie is no longer exempted by metadata,
  // ExcludeThirdPartyPhaseout cookie issue should be present.
  WaitForCookieIssueAndCheck("b.test", {}, {"ExcludeThirdPartyPhaseout"});
}

IN_PROC_BROWSER_TEST_F(
    TpcdMetadataDevtoolsObserverTrackingProtectionDisabledBrowserTest,
    DevToolsDisableMetadataJS) {
  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/true,
                        /*disable_third_party_cookie_heuristics=*/false);

  NavigateToPageWith3pIFrame("a.test");

  // Neither of these commands should work.
  SetCookieFromJS(GetFrame(), "nonExistentCookie=value");
  EXPECT_EQ(ReadCookiesFromJS(GetFrame()), "");

  // Reenabling the metadata exemption. Cookie should now get set.
  SendSetCookieControls(/*enable_third_party_cookie_restriction=*/true,
                        /*disable_third_party_cookie_metadata=*/false,
                        /*disable_third_party_cookie_heuristics=*/false);

  // Refreshing so that RCM is re-created with new controls
  NavigateToPageWith3pIFrame("a.test");

  // Should now be unblocked by metadata and return the new cookie we set.
  SetCookieFromJS(GetFrame(), "cookie=false");
  EXPECT_EQ(ReadCookiesFromJS(GetFrame()), "cookie=false");
}

// Setting the DTRP values in the issue needs to be tested with the flag off.
// Otherwise, a non-zero DTRP value might filter the entry and the issue will
// never fire.
class TpcdMetadataDevtoolsObserverDtrpDisabledBrowserTest
    : public TpcdMetadataDevtoolsObserverBrowserTest {
 public:
  TpcdMetadataDevtoolsObserverDtrpDisabledBrowserTest()
      : TpcdMetadataDevtoolsObserverBrowserTest(
            /*enable_tracking_protection=*/true,
            /*enable_metadata_feature=*/true,
            /*enable_staged_control=*/false) {}
};

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverDtrpDisabledBrowserTest,
                       EmitsDevtoolsIssuesWithDtrpValues) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForMetadataIssueAndCheck({"b.test"}, 50u, true);

  AddCookieAccess("a.test", "c.test", /*is_ad_tagged=*/false);
  WaitForMetadataIssueAndCheck({"c.test"}, 20u, false);
}

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       EmitsDevtoolsIssuesForExemption) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);
  WaitForCookieIssueAndCheck(
      "b.test", /*warning=*/{"WarnDeprecationTrialMetadata"}, /*exclusion=*/{});

  AddCookieAccess("a.test", "c.test", /*is_ad_tagged=*/false);
  WaitForCookieIssueAndCheck(
      "c.test", /*warning=*/{"WarnDeprecationTrialMetadata"}, /*exclusion=*/{});
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
            /*enable_tracking_protection=*/true,
            /*enable_metadata_feature=*/false,
            /*enable_staged_control=*/true) {}
};

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverDisabledBrowserTest,
                       DoesNotEmitDevtoolsIssueWithMissingFeature) {
  AddCookieAccess("a.test", "b.test", /*is_ad_tagged=*/false);

  CheckNoAddedIssue();
}

}  // namespace tpcd::metadata
