// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_cookies_test_helper.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

// static
std::vector<std::string> ExtensionCookiesTestHelper::AsCookies(
    const std::string& cookie_line) {
  return base::SplitString(cookie_line, ";", base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

ExtensionCookiesTestHelper::ExtensionCookiesTestHelper(
    net::test_server::EmbeddedTestServer& test_server,
    Profile& profile,
    std::string csp_header)
    : test_server_(test_server),
      profile_(profile),
      csp_header_(std::move(csp_header)) {
  for (int i = 0; i < kMaxNumberOfCookieRequestsFromSingleTest; i++) {
    http_responses_.push_back(
        std::make_unique<net::test_server::ControllableHttpResponse>(
            &*test_server_, kFetchCookiesPath));
  }
}

ExtensionCookiesTestHelper::~ExtensionCookiesTestHelper() = default;

content::RenderFrameHost* ExtensionCookiesTestHelper::MakeChildFrame(
    content::RenderFrameHost* frame,
    const std::string& host) {
  EXPECT_FALSE(content::ChildFrameAt(frame, 0));
  GURL url = test_server_->GetURL(
      host,
      base::StrCat({"/set-header?Content-Security-Policy: ", csp_header_}));
  const char kAppendFrameScriptTemplate[] = R"(
      var f = document.createElement('iframe');
      f.src = $1;
      new Promise(resolve => {
        f.onload = function(e) {
            resolve(true);
            f.onload = undefined;
        }
        document.body.appendChild(f);
      });
      )";
  std::string append_frame_script =
      content::JsReplace(kAppendFrameScriptTemplate, url.spec());
  EXPECT_EQ(true, content::EvalJs(frame, append_frame_script));
  content::RenderFrameHost* child_frame = content::ChildFrameAt(frame, 0);
  EXPECT_EQ(url, child_frame->GetLastCommittedURL());
  return child_frame;
}

void ExtensionCookiesTestHelper::SetCookies(
    const std::string& host,
    const std::vector<std::string>& cookies) {
  GURL url = test_server_->GetURL(host, "/");
  for (const std::string& cookie : cookies) {
    ASSERT_TRUE(content::SetCookie(&*profile_, url, cookie));
  }
}

std::string ExtensionCookiesTestHelper::FetchCookies(
    content::RenderFrameHost* frame,
    const std::string& host) {
  GURL cookie_url = test_server_->GetURL(host, kFetchCookiesPath);
  const char kFetchCookiesScriptTemplate[] = R"(
      fetch($1, {method: 'GET', credentials: 'include'})
        .then((resp) => resp.text())
        .then((data) => window.domAutomationController.send(data));)";
  std::string fetch_cookies_script =
      content::JsReplace(kFetchCookiesScriptTemplate, cookie_url.spec());
  content::DOMMessageQueue messages(frame);
  ExecuteScriptAsync(frame, fetch_cookies_script);

  url::Origin initiator = frame->GetLastCommittedOrigin();
  WaitForRequestAndRespondWithCookies(initiator);

  std::string result;
  if (!messages.PopMessage(&result)) {
    EXPECT_TRUE(messages.WaitForMessage(&result));
  }
  base::TrimString(result, "\"", &result);
  return result;
}

std::string ExtensionCookiesTestHelper::NavigateChildAndGetCookies(
    content::RenderFrameHost* frame,
    const std::string& host) {
  GURL cookie_url = test_server_->GetURL(host, kFetchCookiesPath);
  url::Origin initiator = frame->GetLastCommittedOrigin();
  // We cache the parent here, and use it to get the RenderFrameHost again
  // later, in order to allow cross-site navigations. Cross-site navigations
  // cause `frame` to be freed (and use a new RFHI for the new document), so
  // it is not safe to use `frame` after the call to `ExecuteScriptAsync`.
  content::RenderFrameHost* parent = frame->GetParent();
  content::TestNavigationObserver nav_observer(
      content::WebContents::FromRenderFrameHost(parent));
  // We assume there's only one child.
  DCHECK_EQ(frame, content::ChildFrameAt(parent, 0));
  ExecuteScriptAsync(frame, content::JsReplace("location = $1", cookie_url));
  WaitForRequestAndRespondWithCookies(initiator);
  nav_observer.Wait();

  return content::EvalJs(content::ChildFrameAt(parent, 0),
                         "document.body.innerText")
      .ExtractString();
}

void ExtensionCookiesTestHelper::WaitForRequestAndRespondWithCookies(
    const url::Origin& initiator) {
  net::test_server::ControllableHttpResponse& http_response =
      GetNextCookieResponse();
  http_response.WaitForRequest();

  // Remove the trailing slash from the URL.
  std::string origin = initiator.GetURL().spec();
  base::TrimString(origin, "/", &origin);

  // Get the 'Cookie' header that was sent in the request.
  std::string cookie_header;
  auto cookie_header_it = http_response.http_request()->headers.find(
      net::HttpRequestHeaders::kCookie);
  if (cookie_header_it == http_response.http_request()->headers.end()) {
    cookie_header = "";
  } else {
    cookie_header = cookie_header_it->second;
  }
  std::string content_length = base::NumberToString(cookie_header.length());

  // clang-format off
  http_response.Send(
      base::StrCat({
      "HTTP/1.1 200 OK\r\n",
      "Content-Type: text/plain; charset=utf-8\r\n",
      "Content-Length: ", content_length, "\r\n",
      "Access-Control-Allow-Origin: ", origin, "\r\n",
      "Access-Control-Allow-Credentials: true\r\n",
      "\r\n",
      cookie_header}));
  // clang-format on

  http_response.Done();
}

net::test_server::ControllableHttpResponse&
ExtensionCookiesTestHelper::GetNextCookieResponse() {
  // If the DCHECK below fails, consider increasing the value of the
  // `kMaxNumberOfCookieRequestsFromSingleTest` constant in the header.
  DCHECK_LT(index_of_active_http_response_, http_responses_.size());

  return *http_responses_[index_of_active_http_response_++];
}

}  // namespace extensions
