// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/test/scoped_feature_list.h"
#include "base/test/values_test_util.h"
#include "chrome/browser/fingerprinting_protection/fingerprinting_protection_filter_browser_test_harness.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/fingerprinting_protection_filter/common/fingerprinting_protection_filter_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "url/gurl.h"

namespace fingerprinting_protection_filter {
namespace {

using testing::Eq;
using testing::Pointee;

class UserReideintificationDevtoolsProtocolTest
    : public content::TestDevToolsProtocolClient,
      public FingerprintingProtectionFilterBrowserTest {
 public:
  UserReideintificationDevtoolsProtocolTest() = default;

  UserReideintificationDevtoolsProtocolTest(
      const UserReideintificationDevtoolsProtocolTest&) = delete;
  UserReideintificationDevtoolsProtocolTest& operator=(
      const UserReideintificationDevtoolsProtocolTest&) = delete;

  ~UserReideintificationDevtoolsProtocolTest() override = default;

  void EnableAudits() {
    Attach();
    SendCommandSync("Audits.enable");
  }

  void WaitForIssueAddedWithProperties(const std::string& type_enum_string,
                                       const GURL& affected_url) {
    auto matcher = [](const base::Value::Dict& params) {
      const std::string* maybe_issue_code =
          params.FindStringByDottedPath("issue.code");
      return maybe_issue_code &&
             *maybe_issue_code == "UserReidentificationIssue";
    };

    base::Value::Dict notification = WaitForMatchingNotification(
        "Audits.issueAdded", base::BindRepeating(matcher));

    EXPECT_EQ(*notification.FindStringByDottedPath(
                  "issue.details.userReidentificationIssueDetails.type"),
              type_enum_string);
    EXPECT_EQ(*notification.FindStringByDottedPath(
                  "issue.details.userReidentificationIssueDetails.request.url"),
              affected_url.possibly_invalid_spec());
  }

 protected:
  void Attach() { AttachToWebContents(web_contents()); }

  // InProcessBrowserTest interface
  void TearDownOnMainThread() override { DetachProtocolClient(); }

  base::test::ScopedFeatureList scoped_feature_list_;
};

class IncognitoUserReideintificationDevtoolsProtocolTest
    : public UserReideintificationDevtoolsProtocolTest {
 public:
  IncognitoUserReideintificationDevtoolsProtocolTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kEnableFingerprintingProtectionFilterInIncognito,
          {{"activation_level", "enabled"}}}},
        /*disabled_features=*/
        {{features::kEnableFingerprintingProtectionFilter}});
  }

  IncognitoUserReideintificationDevtoolsProtocolTest(
      const IncognitoUserReideintificationDevtoolsProtocolTest&) = delete;
  IncognitoUserReideintificationDevtoolsProtocolTest& operator=(
      const IncognitoUserReideintificationDevtoolsProtocolTest&) = delete;

  ~IncognitoUserReideintificationDevtoolsProtocolTest() override = default;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UserReideintificationDevtoolsProtocolTest,
                       Enabled_SubframeDocumentLoadIssueReported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableAudits();

  // Navigate to a test page with multiple child frames and disallow loading
  // child frame documents that in turn would end up loading
  // included_script.html.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html"));
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));
  ASSERT_TRUE(NavigateToDestination(url));

  // Navigate the first subframe to a disallowed URL and verify that an issue is
  // reported.
  GURL disallowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_included_script.html"));
  NavigateFrame(kSubframeNames[0], disallowed_subdocument_url);

  WaitForIssueAddedWithProperties(
      /*type_enum_string=*/"BlockedFrameNavigation",
      /*affected_url=*/GetCrossSiteTestUrl("/frame_with_included_script.html"));
}

IN_PROC_BROWSER_TEST_F(UserReideintificationDevtoolsProtocolTest,
                       Enabled_SubresourceLoadIssueReported) {
  ASSERT_TRUE(embedded_test_server()->Start());
  EnableAudits();
  GURL url = GetTestUrl("/frame_with_cross_origin_included_script.html");

  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("included_script.js"));
  ASSERT_TRUE(NavigateToDestination(url));
  EXPECT_FALSE(
      WasParsedScriptElementLoaded(web_contents()->GetPrimaryMainFrame()));
  WaitForIssueAddedWithProperties(
      /*type_enum_string=*/"BlockedSubresource",
      /*affected_url=*/GURL(
          "https://cross-origin-site.com/included_script.js"));
}

IN_PROC_BROWSER_TEST_F(IncognitoUserReideintificationDevtoolsProtocolTest,
                       Enabled_Incognito_SubframeDocumentLoadIssueReported) {
  // Close normal browser and switch the test's browser instance to an incognito
  // instance.
  Browser* incognito = CreateIncognitoBrowser(browser()->profile());
  CloseBrowserSynchronously(browser());
  SelectFirstBrowser();
  ASSERT_EQ(browser(), incognito);

  ASSERT_TRUE(embedded_test_server()->Start());
  EnableAudits();

  // Navigate to a test page with multiple child frames and disallow loading
  // child frame documents that in turn would end up loading
  // included_script.html.
  ASSERT_NO_FATAL_FAILURE(
      SetRulesetToDisallowURLsWithSubstring("frame_with_included_script.html"));
  GURL url(GetTestUrl(kMultiPlatformTestFrameSetPath));
  ASSERT_TRUE(NavigateToDestination(url));

  // Navigate the first subframe to a disallowed URL and verify that an issue is
  // reported.
  GURL disallowed_subdocument_url(
      GetCrossSiteTestUrl("/frame_with_included_script.html"));
  NavigateFrame(kSubframeNames[0], disallowed_subdocument_url);

  WaitForIssueAddedWithProperties(
      /*type_enum_string=*/"BlockedFrameNavigation",
      /*affected_url=*/GetCrossSiteTestUrl("/frame_with_included_script.html"));
}

}  // namespace fingerprinting_protection_filter
