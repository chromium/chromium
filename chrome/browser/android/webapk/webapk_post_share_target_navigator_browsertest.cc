// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_post_share_target_navigator.h"

#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "chrome/test/base/android/android_browser_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace webapk {
namespace {

// Hosts covered by CERT_TEST_NAMES. `a.test` plays the role of the installed
// web app, `b.test` the role of the unrelated site being attacked.
constexpr char kAppHost[] = "a.test";
constexpr char kCrossSiteHost[] = "b.test";
constexpr char kTargetPath[] = "/echo";

constexpr char kFormEncodedHeader[] =
    "Content-Type: application/x-www-form-urlencoded\r\n";

scoped_refptr<network::ResourceRequestBody> MakeShareBody() {
  return network::ResourceRequestBody::CreateFromCopyOfBytes(
      base::as_byte_span(std::string_view("title=shared")));
}

}  // namespace

// Covers the SameSite treatment of Web Share Target POST navigations. A share
// target action is required to be within the app's scope, but the app may
// redirect it anywhere. Such a navigation must not be treated as if the user
// had typed the destination URL, otherwise the app can force the browser to
// send SameSite=Strict/Lax cookies on a cross-site POST of its choosing. See
// crbug.com/40061291.
class WebApkPostShareTargetNavigatorBrowserTest : public AndroidBrowserTest {
 public:
  WebApkPostShareTargetNavigatorBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    net::test_server::RegisterDefaultHandlers(&https_server_);
    https_server_.RegisterRequestMonitor(base::BindRepeating(
        &WebApkPostShareTargetNavigatorBrowserTest::MonitorRequest,
        base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // Sets one cookie of each SameSite flavour on `url`.
  void SetSameSiteCookies(const GURL& url) {
    ASSERT_TRUE(content::SetCookie(chrome_test_utils::GetProfile(this), url,
                                   "strict=1; SameSite=Strict; Secure"));
    ASSERT_TRUE(content::SetCookie(chrome_test_utils::GetProfile(this), url,
                                   "lax=1; SameSite=Lax; Secure"));
    ASSERT_TRUE(content::SetCookie(chrome_test_utils::GetProfile(this), url,
                                   "none=1; SameSite=None; Secure"));
  }

  // The Cookie header the target endpoint received, or the empty string if it
  // received none.
  std::string target_cookie_header() {
    base::AutoLock lock(lock_);
    return target_cookie_header_;
  }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  // Runs on the embedded test server's thread.
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (!base::StartsWith(request.relative_url, kTargetPath)) {
      return;
    }
    base::AutoLock lock(lock_);
    auto it = request.headers.find("Cookie");
    target_cookie_header_ =
        it == request.headers.end() ? std::string() : it->second;
  }

  base::Lock lock_;
  std::string target_cookie_header_ GUARDED_BY(lock_);
};

// The regression test for crbug.com/40061291: an in-scope action that
// 307-redirects to a cross-site URL must not leak SameSite=Strict/Lax cookies
// belonging to that site.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetNavigatorBrowserTest,
                       CrossSiteRedirectDropsSameSiteCookies) {
  const GURL target = https_server_.GetURL(kCrossSiteHost, kTargetPath);
  ASSERT_NO_FATAL_FAILURE(SetSameSiteCookies(target));

  const GURL action = https_server_.GetURL(
      kAppHost, base::StrCat({"/server-redirect-307?", target.spec()}));

  content::TestNavigationObserver observer(web_contents());
  NavigateShareTargetPost(MakeShareBody(), kFormEncodedHeader, action,
                          web_contents());
  observer.Wait();

  ASSERT_EQ(target, web_contents()->GetLastCommittedURL());

  const std::string cookies = target_cookie_header();
  EXPECT_THAT(cookies, testing::HasSubstr("none=1"));
  EXPECT_THAT(cookies, testing::Not(testing::HasSubstr("strict=1")));
  EXPECT_THAT(cookies, testing::Not(testing::HasSubstr("lax=1")));
}

// The sharing flow itself must keep working: a share target that does not
// redirect is same-site with its own origin and still receives its Strict
// cookies, exactly as if the app had submitted the form itself.
IN_PROC_BROWSER_TEST_F(WebApkPostShareTargetNavigatorBrowserTest,
                       SameSiteShareTargetKeepsStrictCookies) {
  const GURL target = https_server_.GetURL(kAppHost, kTargetPath);
  ASSERT_NO_FATAL_FAILURE(SetSameSiteCookies(target));

  content::TestNavigationObserver observer(web_contents());
  NavigateShareTargetPost(MakeShareBody(), kFormEncodedHeader, target,
                          web_contents());
  observer.Wait();

  ASSERT_EQ(target, web_contents()->GetLastCommittedURL());

  const std::string cookies = target_cookie_header();
  EXPECT_THAT(cookies, testing::HasSubstr("strict=1"));
  EXPECT_THAT(cookies, testing::HasSubstr("lax=1"));
  EXPECT_THAT(cookies, testing::HasSubstr("none=1"));
}

}  // namespace webapk
