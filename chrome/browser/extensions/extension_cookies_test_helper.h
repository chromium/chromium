// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_TEST_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_TEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ref.h"

class Profile;

namespace content {
class RenderFrameHost;
}  // namespace content

namespace net::test_server {
class ControllableHttpResponse;
class EmbeddedTestServer;
}  // namespace net::test_server

namespace url {
class Origin;
}  // namespace url

namespace extensions {

// Provides shared cookie test helpers (request fixtures, frame setup,
// cookie store seeding) backed by a `ControllableHttpResponse` pool. Does
// NOT own the `EmbeddedTestServer` or the `Profile` -- both are passed by
// reference at construction; tests must not exceed
// `kMaxNumberOfCookieRequestsFromSingleTest` cookie fetches. Must be
// constructed before the associated `EmbeddedTestServer` is started so
// the controllable response pool can be installed.
class ExtensionCookiesTestHelper {
 public:
  // Hosts and permission patterns shared by both cookie test fixtures.
  static constexpr char kPermittedHost[] = "a.example";
  static constexpr char kOtherPermittedHost[] = "b.example";
  static constexpr char kNotPermittedHost[] = "c.example";
  static constexpr char kPermittedSubdomain[] = "sub.a.example";
  static constexpr char kNotPermittedSubdomain[] = "notallowedsub.a.example";
  static constexpr char kCrossOriginHost[] = "other.com";
  static constexpr char kPermissionPattern1[] = "https://a.example/*";
  static constexpr char kPermissionPattern1Sub[] = "https://sub.a.example/*";
  static constexpr char kPermissionPattern2[] = "https://b.example/*";

  // Path served by the helper's `ControllableHttpResponse` pool.
  static constexpr char kFetchCookiesPath[] = "/respondwithcookies";

  // Cookie name + SameSite attribute fixtures.
  static constexpr char kNoneCookie[] = "none=1";
  static constexpr char kLaxCookie[] = "lax=1";
  static constexpr char kStrictCookie[] = "strict=1";
  static constexpr char kUnspecifiedCookie[] = "unspecified=1";
  static constexpr char kSameSiteNoneAttribute[] = "; SameSite=None; Secure";
  static constexpr char kSameSiteLaxAttribute[] = "; SameSite=Lax";
  static constexpr char kSameSiteStrictAttribute[] = "; SameSite=Strict";

  // Splits "a=1; b=2" into {"a=1","b=2"}.
  static std::vector<std::string> AsCookies(const std::string& cookie_line);

  // `csp_header` is captured by value because the upstream and MIME handler
  // fixtures use different CSP literals -- preserving each verbatim keeps
  // wire behavior unchanged. The constructor allocates the
  // `ControllableHttpResponse` pool against `test_server`, so it must run
  // before the associated `EmbeddedTestServer` is started.
  ExtensionCookiesTestHelper(net::test_server::EmbeddedTestServer& test_server,
                             Profile& profile,
                             std::string csp_header);
  ExtensionCookiesTestHelper(const ExtensionCookiesTestHelper&) = delete;
  ExtensionCookiesTestHelper& operator=(const ExtensionCookiesTestHelper&) =
      delete;
  ~ExtensionCookiesTestHelper();

  // Appends a child iframe under `frame` pointing at `host`. The child URL
  // responds with `csp_header_` so script execution is allowed across the
  // test hosts.
  content::RenderFrameHost* MakeChildFrame(content::RenderFrameHost* frame,
                                           const std::string& host);

  // Sets `cookies` on `host` directly via the cookie store on `profile_`.
  void SetCookies(const std::string& host,
                  const std::vector<std::string>& cookies);

  // Issues `fetch` against `host` from `frame`; returns the Cookie header
  // observed on the wire by consuming the next slot in the response pool.
  std::string FetchCookies(content::RenderFrameHost* frame,
                           const std::string& host);

  // Triggers a `frame`-initiated navigation of `frame` to `host`. Returns
  // the cookies that were sent on that navigation request (read from the
  // navigated child's body).
  std::string NavigateChildAndGetCookies(content::RenderFrameHost* frame,
                                         const std::string& host);

  // Responds to the next pending request with a body that echoes the
  // observed Cookie header. CORS headers permit credentialed cross-origin
  // fetches; `initiator` controls `Access-Control-Allow-Origin`.
  void WaitForRequestAndRespondWithCookies(const url::Origin& initiator);

 private:
  net::test_server::ControllableHttpResponse& GetNextCookieResponse();

  static constexpr int kMaxNumberOfCookieRequestsFromSingleTest = 15;

  const raw_ref<net::test_server::EmbeddedTestServer> test_server_;
  const raw_ref<Profile> profile_;
  const std::string csp_header_;
  std::vector<std::unique_ptr<net::test_server::ControllableHttpResponse>>
      http_responses_;
  size_t index_of_active_http_response_ = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIES_TEST_HELPER_H_
