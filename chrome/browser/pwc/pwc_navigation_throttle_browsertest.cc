// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
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
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(https_server_.Start());
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
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

// Nested schemes (blob:, filesystem:) whose inner origin matches the allowlist
// are still rejected for the primary main frame because the navigation URL
// itself does not use an HTTP(S) scheme.
IN_PROC_BROWSER_TEST_F(PwcNavigationThrottleBrowserTest,
                       CancelsNestedSchemeMainFrameNavigations) {
  const GURL allowed = https_server_.GetURL("a.test", "/allowed.html");
  const GURL allowed_next = https_server_.GetURL("a.test", "/next.html");
  const url::Origin allowed_origin = url::Origin::Create(allowed);

  std::unique_ptr<PrivilegedWebContents> privileged =
      PrivilegedWebContents::Create(
          PrivilegedComponent::kTestComponent, browser()->GetProfile(),
          std::make_unique<FixedPwcPolicyDelegate>(
              std::vector<url::Origin>{allowed_origin},
              std::vector<url::Origin>{allowed_origin}));
  content::WebContents* web_contents = privileged->web_contents();

  ASSERT_TRUE(content::NavigateToURL(web_contents, allowed));
  ASSERT_EQ(allowed, web_contents->GetLastCommittedURL());

  // Create a live blob: URL on the allowlisted origin. url::Origin::Create()
  // unwraps it to `allowed_origin`, but its outer scheme is blob:.
  const GURL blob_url(
      content::EvalJs(
          web_contents,
          "URL.createObjectURL(new Blob(['<!doctype html><p>blob</p>'], "
          "{type: 'text/html'}))")
          .ExtractString());
  ASSERT_TRUE(blob_url.is_valid());
  ASSERT_TRUE(blob_url.SchemeIsBlob());
  ASSERT_EQ(allowed_origin, url::Origin::Create(blob_url));

  // Browser-initiated navigation to the blob: URL is cancelled.
  EXPECT_FALSE(content::NavigateToURL(web_contents, blob_url));
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());

  // Renderer-initiated navigation to the blob: URL is also cancelled.
  content::TestNavigationManager blob_nav(web_contents, blob_url);
  ASSERT_TRUE(content::ExecJs(
      web_contents, content::JsReplace("location.href = $1;", blob_url)));
  ASSERT_TRUE(blob_nav.WaitForNavigationFinished());
  EXPECT_FALSE(blob_nav.was_committed());
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());
  EXPECT_EQ(allowed_origin,
            web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());

  // A filesystem: URL wrapping the allowlisted origin is likewise cancelled.
  const GURL filesystem_url(base::StrCat(
      {"filesystem:", allowed.GetWithEmptyPath().spec(), "temporary/a.html"}));
  ASSERT_TRUE(filesystem_url.is_valid());
  ASSERT_TRUE(filesystem_url.SchemeIsFileSystem());
  ASSERT_EQ(allowed_origin, url::Origin::Create(filesystem_url));
  EXPECT_FALSE(content::NavigateToURL(web_contents, filesystem_url));
  EXPECT_EQ(allowed, web_contents->GetLastCommittedURL());

  // Standard HTTPS navigation on the allowlisted origin continues to succeed.
  EXPECT_TRUE(content::NavigateToURL(web_contents, allowed_next));
  EXPECT_EQ(allowed_next, web_contents->GetLastCommittedURL());
}

}  // namespace
}  // namespace pwc
