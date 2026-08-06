// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "chrome/browser/pwc/pwc_features.mojom-features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
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

class PwcNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  PwcNavigationThrottleBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        mojom::features::kPrivilegedWebContents);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// The throttle cancels off-allowlist primary-main-frame navigations in a
// PrivilegedWebContents, while allowlisted ones proceed.
IN_PROC_BROWSER_TEST_F(PwcNavigationThrottleBrowserTest,
                       LocksPrimaryMainFrameToAllowlist) {
  const GURL allowed = https_server_.GetURL("a.test", "/allowed.html");
  const GURL blocked = https_server_.GetURL("b.test", "/blocked.html");

  std::unique_ptr<PrivilegedWebContents> privileged =
      PrivilegedWebContents::Create(
          PrivilegedComponent::kTestComponent, browser()->GetProfile(),
          std::make_unique<FixedPwcPolicyDelegate>(
              std::vector<url::Origin>{url::Origin::Create(allowed)},
              std::vector<url::Origin>{url::Origin::Create(allowed)}));
  content::WebContents* web_contents = privileged->web_contents();

  // A navigation to an allowlisted origin commits.
  EXPECT_TRUE(content::NavigateToURL(web_contents, allowed));
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());

  // A navigation to an off-allowlist origin is cancelled, leaving the committed
  // URL unchanged.
  EXPECT_FALSE(content::NavigateToURL(web_contents, blocked));
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());
}

// A subframe cannot move an off-allowlist origin into the main frame by
// navigating it to about:blank. Such a navigation commits without a URL loader
// (so it never reaches WillStartRequest) and about:blank would inherit the
// subframe's origin; the throttle cancels it via WillCommitWithoutUrlLoader.
IN_PROC_BROWSER_TEST_F(PwcNavigationThrottleBrowserTest,
                       CancelsAboutBlankMainFrameNavigationFromSubframe) {
  const GURL allowed = https_server_.GetURL("a.test", "/allowed.html");
  const GURL subframe_url = https_server_.GetURL("b.test", "/subframe.html");

  std::unique_ptr<PrivilegedWebContents> privileged =
      PrivilegedWebContents::Create(
          PrivilegedComponent::kTestComponent, browser()->GetProfile(),
          std::make_unique<FixedPwcPolicyDelegate>(
              std::vector<url::Origin>{url::Origin::Create(allowed)},
              std::vector<url::Origin>{url::Origin::Create(allowed)}));
  content::WebContents* web_contents = privileged->web_contents();

  // The main frame commits the allowlisted origin and embeds an off-allowlist,
  // cross-origin subframe (subframes are not throttled).
  ASSERT_TRUE(content::NavigateToURL(web_contents, allowed));
  ASSERT_TRUE(content::ExecJs(web_contents, content::JsReplace(R"(
    new Promise((resolve) => {
      const f = document.createElement('iframe');
      f.src = $1;
      f.onload = () => resolve(true);
      document.body.appendChild(f);
    })
  )",
                                                               subframe_url)));
  content::RenderFrameHost* subframe =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(subframe);
  ASSERT_EQ(url::Origin::Create(subframe_url),
            subframe->GetLastCommittedOrigin());

  // The subframe navigates the main frame to about:blank. The throttle cancels
  // it, so the main frame stays on the allowlisted origin.
  content::TestNavigationManager nav_manager(web_contents, GURL("about:blank"));
  ASSERT_TRUE(content::ExecJs(subframe, "top.location.href = 'about:blank';"));
  ASSERT_TRUE(nav_manager.WaitForNavigationFinished());
  EXPECT_FALSE(nav_manager.was_committed());
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());
  EXPECT_EQ(url::Origin::Create(allowed),
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

}  // namespace
}  // namespace pwc
