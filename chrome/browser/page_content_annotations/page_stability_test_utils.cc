// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_stability_test_utils.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/page_content_annotations/content/mojom/page_stability.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace page_content_annotations {

namespace {

using ::content::EvalJs;
using ::content::RenderFrameHost;
using ::content::WebContents;

// Note: this file doesn't actually exist, the response is manually provided by
// tests.
const char kFetchPath[] = "/fetchtarget.html";

}  // namespace

PageStabilityBrowserTestBase::PageStabilityBrowserTestBase() = default;

PageStabilityBrowserTestBase::~PageStabilityBrowserTestBase() = default;

void PageStabilityBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  fetch_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), kFetchPath);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(embedded_https_test_server().Start());
}

void PageStabilityBrowserTestBase::Sleep(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

WebContents* PageStabilityBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

RenderFrameHost* PageStabilityBrowserTestBase::main_frame() {
  return web_contents()->GetPrimaryMainFrame();
}

GURL PageStabilityBrowserTestBase::GetPageStabilityTestURL() {
  return embedded_test_server()->GetURL(
      "/page_content_annotations/page_stability.html");
}

std::string PageStabilityBrowserTestBase::GetOutputText() {
  return EvalJs(web_contents(), "document.getElementById('output').innerText")
      .ExtractString();
}

net::test_server::ControllableHttpResponse&
PageStabilityBrowserTestBase::fetch_response() {
  return *fetch_response_;
}

void PageStabilityBrowserTestBase::InitiateNetworkRequest() {
  ASSERT_TRUE(ExecJs(web_contents(), "window.doFetch(() => {})"));
  fetch_response().WaitForRequest();
}

void PageStabilityBrowserTestBase::Respond(std::string_view text) {
  fetch_response_->Send(net::HTTP_OK, /*content_type=*/"text/html",
                        /*content=*/"",
                        /*cookies=*/{}, /*extra_headers=*/{});
  fetch_response_->Send(std::string(text));
  fetch_response_->Done();
}

}  // namespace page_content_annotations
