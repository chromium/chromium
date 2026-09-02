// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pwc/privileged_web_contents.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/back_forward_cache/back_forward_cache_disable.h"
#include "components/back_forward_cache/disabled_reason_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace pwc {
namespace {

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content_type("text/html");
  response->set_content("<html><body>ok</body></html>");
  return response;
}

class PrivilegedWebContentsBrowserTest : public InProcessBrowserTest {
 public:
  PrivilegedWebContentsBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<PrivilegedWebContents> MakePrivileged(const GURL& allowed) {
    return PrivilegedWebContents::Create(
        PrivilegedComponent::kTestComponent, browser()->GetProfile(),
        std::make_unique<FixedPwcPolicyDelegate>(
            std::vector<url::Origin>{url::Origin::Create(allowed)},
            std::vector<url::Origin>{url::Origin::Create(allowed)}));
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A privileged WebContents cannot create related windows: window.open() returns
// null, because ChromeContentBrowserClient::CanCreateWindow denies it.
IN_PROC_BROWSER_TEST_F(PrivilegedWebContentsBrowserTest,
                       WindowOpenReturnsNull) {
  const GURL allowed = https_server_.GetURL("a.test", "/allowed.html");
  std::unique_ptr<PrivilegedWebContents> privileged = MakePrivileged(allowed);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, allowed));

  // window.open() to any URL yields null in a privileged WebContents.
  EXPECT_EQ(false,
            content::EvalJs(web_contents,
                            "!!window.open('https://a.test/', '_blank');"));
  EXPECT_EQ(false,
            content::EvalJs(web_contents, "!!window.open('', '_blank');"));

  // An ordinary tab can still open a window (control).
  ASSERT_TRUE(content::NavigateToURL(
      browser()->tab_strip_model()->GetActiveWebContents(), allowed));
  EXPECT_EQ(true, content::EvalJs(
                      browser()->tab_strip_model()->GetActiveWebContents(),
                      "!!window.open('https://a.test/', '_blank');"));
}

// A privileged WebContents keeps its committed documents out of the
// back-forward cache: the committing frame is recorded as bfcache-disabled with
// the privileged reason.
IN_PROC_BROWSER_TEST_F(PrivilegedWebContentsBrowserTest,
                       DisablesBackForwardCache) {
  const GURL allowed = https_server_.GetURL("a.test", "/allowed.html");
  content::BackForwardCacheDisabledTester tester;
  std::unique_ptr<PrivilegedWebContents> privileged = MakePrivileged(allowed);
  content::WebContents* web_contents = privileged->web_contents();
  ASSERT_TRUE(content::NavigateToURL(web_contents, allowed));

  content::RenderFrameHost* rfh = web_contents->GetPrimaryMainFrame();
  EXPECT_TRUE(tester.IsDisabledForFrameWithReason(
      rfh->GetProcess()->GetDeprecatedID(), rfh->GetRoutingID(),
      back_forward_cache::DisabledReason(
          back_forward_cache::DisabledReasonId::kPrivilegedWebContents)));

  // Navigate away to another allowed page, then go back. Because the first
  // document was kept out of the back-forward cache, navigating away must
  // destroy its RenderFrameHost (rather than cache it), and going back must
  // load a fresh document -- proving the page was not restored from bfcache.
  const GURL allowed2 = https_server_.GetURL("a.test", "/allowed2.html");
  content::RenderFrameHostWrapper rfh_wrapper(rfh);
  ASSERT_TRUE(content::NavigateToURL(web_contents, allowed2));
  EXPECT_TRUE(rfh_wrapper.WaitUntilRenderFrameDeleted());

  ASSERT_TRUE(content::HistoryGoBack(web_contents));
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());
}

}  // namespace
}  // namespace pwc
