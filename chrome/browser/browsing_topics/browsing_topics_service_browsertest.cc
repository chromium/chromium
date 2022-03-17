// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browsing_topics/browsing_topics_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/browsing_topics/browsing_topics_service.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

class BrowsingTopicsBrowserTestBase : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());

    content::SetupCrossSiteRedirector(&https_server_);
    ASSERT_TRUE(https_server_.Start());

    content::SetupCrossSiteRedirector(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  std::string InvokeTopicsAPI(const content::ToRenderFrameHost& adapter) {
    return EvalJs(adapter, R"(
      if (!(document.browsingTopics instanceof Function)) {
        'not a function';
      } else {
        document.browsingTopics()
        .then(topics => {
          let result = "[";
          for (const topic of topics) {
            result += JSON.stringify(topic, Object.keys(topic).sort()) + ";"
          }
          result += "]";
          return result;
        })
        .catch(error => error.message);
      }
    )")
        .ExtractString();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

 protected:
  net::EmbeddedTestServer https_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTPS};
};

class BrowsingTopicsDisabledBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{blink::features::kBrowsingTopics});
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest,
                       NoBrowsingTopicsService) {
  EXPECT_FALSE(
      BrowsingTopicsServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsDisabledBrowserTest, NoTopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("not a function", InvokeTopicsAPI(web_contents()));
}

class BrowsingTopicsBrowserTest : public BrowsingTopicsBrowserTestBase {
 public:
  BrowsingTopicsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{blink::features::kBrowsingTopics},
        /*disabled_features=*/{});
  }

  browsing_topics::BrowsingTopicsService* browsing_topics_service() {
    return BrowsingTopicsServiceFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, HasBrowsingTopicsService) {
  EXPECT_TRUE(browsing_topics_service());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, NoServiceInIncognitoMode) {
  CreateIncognitoBrowser(browser()->profile());

  EXPECT_TRUE(browser()->profile()->HasPrimaryOTRProfile());

  Profile* incognito_profile =
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/false);
  EXPECT_TRUE(incognito_profile);

  BrowsingTopicsService* incognito_browsing_topics_service =
      BrowsingTopicsServiceFactory::GetForProfile(incognito_profile);
  EXPECT_FALSE(incognito_browsing_topics_service);
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, BrowsingTopicsStateOnStart) {
  EXPECT_TRUE(browsing_topics_service()
                  ->GetTopicsForSiteForDisplay(/*top_origin=*/url::Origin())
                  .empty());
  EXPECT_TRUE(browsing_topics_service()->GetTopTopicsForDisplay().empty());
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest, EmptyPage_TopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("[]", InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_browsing_topics_none.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    EmptyPage_PermissionsPolicyInterestCohortNone_TopicsAPI) {
  GURL main_frame_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_interest_cohort_none.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "The \"interest-cohort\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(web_contents()));
}

IN_PROC_BROWSER_TEST_F(
    BrowsingTopicsBrowserTest,
    OneIframePage_SubframePermissionsPolicyBrowsingTopicsNone_TopicsAPI) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  GURL subframe_url = https_server_.GetURL(
      "a.test", "/browsing_topics/empty_page_browsing_topics_none.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  EXPECT_EQ("[]", InvokeTopicsAPI(web_contents()));

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       PermissionsPolicyAllowCertainOrigin_TopicsAPI) {
  base::StringPairs port_replacement;
  port_replacement.push_back(
      std::make_pair("{{PORT}}", base::NumberToString(https_server_.port())));

  GURL main_frame_url = https_server_.GetURL(
      "a.test", net::test_server::GetFilePathWithReplacements(
                    "/browsing_topics/"
                    "one_iframe_page_browsing_topics_allow_certain_origin.html",
                    port_replacement));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ("[]", InvokeTopicsAPI(web_contents()));

  GURL subframe_url =
      https_server_.GetURL("c.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));
  EXPECT_EQ("[]", InvokeTopicsAPI(content::ChildFrameAt(
                      web_contents()->GetMainFrame(), 0)));

  subframe_url =
      https_server_.GetURL("b.test", "/browsing_topics/empty_page.html");

  ASSERT_TRUE(content::NavigateIframeToURL(web_contents(),
                                           /*iframe_id=*/"frame",
                                           subframe_url));

  EXPECT_EQ(
      "The \"browsing-topics\" Permissions Policy denied the use of "
      "document.browsingTopics().",
      InvokeTopicsAPI(
          content::ChildFrameAt(web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInInsecureContext) {
  GURL main_frame_url = embedded_test_server()->GetURL(
      "a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  // Navigate the iframe to a https site.
  GURL subframe_url = https_server_.GetURL("b.test", "/empty_page.html");
  content::NavigateIframeToURL(web_contents(),
                               /*iframe_id=*/"frame", subframe_url);

  // Both the main frame and the subframe are insecure context because the main
  // frame is loaded over HTTP. Expect that the API isn't available in either
  // frame.
  EXPECT_EQ("not a function", InvokeTopicsAPI(web_contents()));
  EXPECT_EQ("not a function", InvokeTopicsAPI(content::ChildFrameAt(
                                  web_contents()->GetMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(BrowsingTopicsBrowserTest,
                       TopicsAPINotAllowedInDetachedDocument) {
  GURL main_frame_url =
      https_server_.GetURL("a.test", "/browsing_topics/one_iframe_page.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_frame_url));

  EXPECT_EQ(
      "Failed to execute 'browsingTopics' on 'Document': A browsing "
      "context is required when calling document.browsingTopics().",
      EvalJs(web_contents(), R"(
      const iframe = document.getElementById('frame');
      const childDocument = iframe.contentWindow.document;
      iframe.remove();

      childDocument.browsingTopics()
        .then(topics => "success")
        .catch(error => error.message);
    )"));
}

}  // namespace browsing_topics
