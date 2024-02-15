// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/files/file_path.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/dips/dips_service.h"
#include "chrome/browser/dips/dips_test_utils.h"
#include "chrome/browser/tpcd/metadata/devtools_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/tpcd/metadata/parser.h"
#include "components/tpcd/metadata/parser_test_helper.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"

namespace tpcd::metadata {
namespace {

using ::chrome_test_utils::GetActiveWebContents;

}  // namespace

class TpcdMetadataDevtoolsObserverBrowserTest
    : public PlatformBrowserTest,
      public content::TestDevToolsProtocolClient {
 public:
  explicit TpcdMetadataDevtoolsObserverBrowserTest(
      bool enable_metadata_feature = true)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_list_.InitWithFeatureStates(
        {{content_settings::features::kTrackingProtection3pcd, true},
         {net::features::kTpcdMetadataGrants, enable_metadata_feature}});
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
    std::vector<MetadataPair> metadata_pairs;
    const std::string first_party_pattern_spec = "[*.]a.test";
    const std::string third_party_pattern_spec_1 = "[*.]b.test";
    const std::string third_party_pattern_spec_2 = "c.test";
    metadata_pairs.emplace_back(third_party_pattern_spec_1,
                                first_party_pattern_spec);
    metadata_pairs.emplace_back(third_party_pattern_spec_2,
                                first_party_pattern_spec);
    Metadata metadata = MakeMetadataProtoFromVectorOfPair(metadata_pairs);
    tpcd::metadata::Parser::GetInstance()->ParseMetadata(
        metadata.SerializeAsString());

    // Initialize devtools WebContentsObserver.
    devtools_observer_ = TpcdMetadataDevtoolsObserver::FromWebContents(
        GetActiveWebContents(this));
  }

  void TearDownOnMainThread() override {
    DetachProtocolClient();
    devtools_observer_ = nullptr;
  }

 protected:
  void AddCookieAccess(const std::string& first_party_site,
                       const std::string& third_party_site) {
    ASSERT_TRUE(NavigateToSetCookie(GetActiveWebContents(this), &https_server_,
                                    third_party_site,
                                    /*is_secure_cookie_set=*/true));
    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(this),
        embedded_test_server()->GetURL(first_party_site, "/title1.html")));
    CreateImageAndWaitForCookieAccess(
        GetActiveWebContents(this),
        https_server_.GetURL(third_party_site, "/favicon/icon.png"));
  }

  void WaitForIssueAndCheckAllowedSites(const std::vector<std::string>& sites) {
    auto is_metadata_issue = [](const base::Value::Dict& params) {
      return *(params.FindStringByDottedPath("issue.code")) ==
             "CookieDeprecationMetadataIssue";
    };

    // Wait for notification of a Metadata Issue.
    base::Value::Dict params = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(is_metadata_issue));
    ASSERT_EQ(*params.FindStringByDottedPath("issue.code"),
              "CookieDeprecationMetadataIssue");

    base::Value::Dict* metadata_issue_details = params.FindDictByDottedPath(
        "issue.details.cookieDeprecationMetadataIssueDetails");
    ASSERT_TRUE(metadata_issue_details);

    std::vector<std::string> allowed_sites;
    base::Value::List* allowed_sites_list =
        metadata_issue_details->FindList("allowedSites");
    if (allowed_sites_list) {
      for (const auto& val : *allowed_sites_list) {
        allowed_sites.push_back(val.GetString());
      }
    }

    // Verify the reported allowed sites match the expected sites.
    EXPECT_THAT(allowed_sites, testing::ElementsAreArray(sites));

    // Clear existing notifications so subsequent calls don't fail by checking
    // `sites` against old notifications.
    ClearNotifications();
  }

  void CheckNoAddedIssue() {
    ReportDummyIssue();

    WaitForIssueAndCheckAllowedSites({"dummy.test"});
  }

 private:
  void ReportDummyIssue() {
    auto details = blink::mojom::InspectorIssueDetails::New();

    auto metadata_issue_details =
        blink::mojom::CookieDeprecationMetadataIssueDetails::New();
    metadata_issue_details->allowed_sites.push_back("dummy.test");
    details->cookie_deprecation_metadata_issue_details =
        std::move(metadata_issue_details);

    GetActiveWebContents(this)->GetPrimaryMainFrame()->ReportInspectorIssue(
        blink::mojom::InspectorIssueInfo::New(
            blink::mojom::InspectorIssueCode::kCookieDeprecationMetadataIssue,
            std::move(details)));
  }

  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_;
  raw_ptr<TpcdMetadataDevtoolsObserver> devtools_observer_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       EmitsDevtoolsIssues) {
  AddCookieAccess("a.test", "b.test");
  WaitForIssueAndCheckAllowedSites({"b.test"});

  AddCookieAccess("a.test", "c.test");
  WaitForIssueAndCheckAllowedSites({"c.test"});
}

IN_PROC_BROWSER_TEST_F(TpcdMetadataDevtoolsObserverBrowserTest,
                       DoesNotEmitDevtoolsIssueForSiteNotInAllowlist) {
  AddCookieAccess("b.test", "a.test");

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
  AddCookieAccess("a.test", "b.test");

  CheckNoAddedIssue();
}

}  // namespace tpcd::metadata
