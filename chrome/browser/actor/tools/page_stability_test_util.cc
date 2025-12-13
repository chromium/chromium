// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/page_stability_test_util.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
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

namespace actor {

namespace {

using ::content::EvalJs;
using ::content::RenderFrameHost;
using ::content::WebContents;

// Note: this file doesn't actually exist, the response is manually provided by
// tests.
const char kFetchPath[] = "/fetchtarget.html";

}  // namespace

PageStabilityTest::PageStabilityTest() {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kGlic, features::kTabstripComboButton,
                            features::kGlicActor},
      /*disabled_features=*/{features::kGlicWarming});
}

PageStabilityTest::~PageStabilityTest() = default;

void PageStabilityTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
  fetch_response_ =
      std::make_unique<net::test_server::ControllableHttpResponse>(
          embedded_test_server(), kFetchPath);

  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(embedded_https_test_server().Start());
}

void PageStabilityTest::Sleep(base::TimeDelta delta) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), delta);
  run_loop.Run();
}

WebContents* PageStabilityTest::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

RenderFrameHost* PageStabilityTest::main_frame() {
  return web_contents()->GetPrimaryMainFrame();
}

GURL PageStabilityTest::GetPageStabilityTestURL() {
  return embedded_test_server()->GetURL("/actor/page_stability.html");
}

std::string PageStabilityTest::GetOutputText() {
  return EvalJs(web_contents(), "document.getElementById('output').innerText")
      .ExtractString();
}

net::test_server::ControllableHttpResponse&
PageStabilityTest::fetch_response() {
  return *fetch_response_;
}

void PageStabilityTest::InitiateNetworkRequest() {
  ASSERT_TRUE(ExecJs(web_contents(), "window.doFetch(() => {})"));
  fetch_response().WaitForRequest();
}

void PageStabilityTest::Respond(std::string_view text) {
  fetch_response_->Send(net::HTTP_OK, /*content_type=*/"text/html",
                        /*content=*/"",
                        /*cookies=*/{}, /*extra_headers=*/{});
  fetch_response_->Send(std::string(text));
  fetch_response_->Done();
}

mojo::Remote<mojom::PageStabilityMonitor>
PageStabilityTest::CreatePageStabilityMonitor(bool supports_paint_stability) {
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  main_frame()->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);

  mojo::Remote<mojom::PageStabilityMonitor> monitor_remote;
  chrome_render_frame->CreatePageStabilityMonitor(
      monitor_remote.BindNewPipeAndPassReceiver(), actor::TaskId(),
      supports_paint_stability);

  // Ensure the monitor is created in the renderer before returning it.
  monitor_remote.FlushForTesting();

  return monitor_remote;
}

}  // namespace actor
