// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/guid.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pdf_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/find_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/common/constants.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/buildflags/buildflags.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif
#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

using extensions::ExtensionsAPIClient;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

// Note: This file contains several old WebViewGuest tests which were for
// certain BrowserPlugin features and no longer made sense for the new
// WebViewGuest which is based on cross-process frames. Since
// MimeHandlerViewGuest is the only guest which still uses BrowserPlugin, the
// test were moved, with adaptation, to this file. Eventually this file might
// contain new tests for MimeHandlerViewGuest but ideally they should all be
// tests which are a) based on cross-process frame version of MHVG, and b) tests
// that need chrome layer API. Anything else should go to the extension layer
// version of the tests. Most of the legacy tests will probably be removed when
// MimeHandlerViewGuest starts using cross-process frames (see
// https://crbug.com/659750).

// Base class for tests below.
class ChromeMimeHandlerViewTest : public extensions::ExtensionApiTest {
 public:
  ChromeMimeHandlerViewTest() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~ChromeMimeHandlerViewTest() override {}

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 protected:
  TestGuestViewManager* GetGuestViewManager() {
    TestGuestViewManager* manager = static_cast<TestGuestViewManager*>(
        TestGuestViewManager::FromBrowserContext(browser()->profile()));
    // TestGuestViewManager::WaitForSingleGuestCreated can and will get called
    // before a guest is created. Since GuestViewManager is usually not created
    // until the first guest is created, this means that |manager| will be
    // nullptr if trying to use the manager to wait for the first guest. Because
    // of this, the manager must be created here if it does not already exist.
    if (!manager) {
      manager = static_cast<TestGuestViewManager*>(
          GuestViewManager::CreateWithDelegate(
              browser()->profile(),
              ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate(
                  browser()->profile())));
    }
    return manager;
  }

  void InitializeTestPage(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    GetGuestViewManager()->RegisterTestGuestViewType<MimeHandlerViewGuest>(
        base::BindRepeating(&TestMimeHandlerViewGuest::Create));

    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    ASSERT_TRUE(extension);
    ASSERT_EQ(std::string(extension_misc::kMimeHandlerPrivateTestExtensionId),
              extension->id());

    extensions::ResultCatcher catcher;
    ui_test_utils::NavigateToURL(browser(), url);

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();

    guest_web_contents_ = GetGuestViewManager()->WaitForSingleGuestCreated();
    embedder_web_contents_ = browser()->tab_strip_model()->GetWebContentsAt(0);
    ASSERT_TRUE(guest_web_contents_);
    ASSERT_TRUE(embedder_web_contents_);
  }

  content::WebContents* guest_web_contents() const {
    return guest_web_contents_;
  }
  content::WebContents* embedder_web_contents() const {
    return embedder_web_contents_;
  }

  // Creates a bogus StreamContainer for the first tab. This is not intended to
  // be really consumed by MimeHandler API.
  std::unique_ptr<extensions::StreamContainer> CreateFakeStreamContainer(
      const GURL& url,
      std::string* view_id) {
    *view_id = base::GenerateGUID();
    auto transferrable_loader = blink::mojom::TransferrableURLLoader::New();
    transferrable_loader->url = url;
    transferrable_loader->head = network::mojom::URLResponseHead::New();
    transferrable_loader->head->mime_type = "application/pdf";
    transferrable_loader->head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/2 200 OK");
    const std::string extension_id =
        extension_misc::kMimeHandlerPrivateTestExtensionId;
    return std::make_unique<extensions::StreamContainer>(
        0 /* tab_id */, false /* embedded */,
        extensions::Extension::GetBaseURLFromExtensionId(
            extension_id) /* handler_url */,
        extension_id, std::move(transferrable_loader), url);
  }

 private:
  TestGuestViewManagerFactory factory_;
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;

  ChromeMimeHandlerViewTest(const ChromeMimeHandlerViewTest&) = delete;
  ChromeMimeHandlerViewTest& operator=(const ChromeMimeHandlerViewTest&) =
      delete;
};

namespace {

// A DevToolsAgentHostClient implementation doing nothing.
class StubDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  StubDevToolsAgentHostClient() {}
  ~StubDevToolsAgentHostClient() override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
};

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
class PrintPreviewDelegate : printing::PrintPreviewUI::TestDelegate {
 public:
  PrintPreviewDelegate() {
    printing::PrintPreviewUI::SetDelegateForTesting(this);
  }
  PrintPreviewDelegate(const PrintPreviewDelegate&) = delete;
  PrintPreviewDelegate& operator=(const PrintPreviewDelegate&) = delete;
  ~PrintPreviewDelegate() override {
    printing::PrintPreviewUI::SetDelegateForTesting(nullptr);
  }

  void WaitUntilPreviewIsReady() {
    if (total_page_count_ > 0)
      return;

    base::RunLoop run_loop;
    quit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // PrintPreviewUI::TestDelegate:
  void DidGetPreviewPageCount(uint32_t page_count) override {
    EXPECT_GE(page_count, 1u);
    total_page_count_ = page_count;
    if (quit_callback_)
      std::move(quit_callback_).Run();
  }
  void DidRenderPreviewPage(content::WebContents* preview_dialog) override {}

  uint32_t total_page_count_ = 0;
  base::OnceClosure quit_callback_;
};
#endif

}  // namespace

// Flaky (https://crbug.com/1033009)
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       DISABLED_GuestDevToolsReloadsEmbedder) {
  GURL data_url("data:application/pdf,foo");
  ui_test_utils::NavigateToURL(browser(), data_url);
  auto* embedder_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* guest_web_contents = GetGuestViewManager()->WaitForSingleGuestCreated();
  EXPECT_NE(embedder_web_contents, guest_web_contents);
  while (guest_web_contents->IsLoading()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  // Load DevTools.
  scoped_refptr<content::DevToolsAgentHost> devtools_agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(guest_web_contents);
  StubDevToolsAgentHostClient devtools_agent_host_client;
  devtools_agent_host->AttachClient(&devtools_agent_host_client);

  // Reload via guest's DevTools, embedder should reload.
  content::TestNavigationObserver reload_observer(embedder_web_contents);
  constexpr char kMsg[] = R"({"id":1,"method":"Page.reload"})";
  devtools_agent_host->DispatchProtocolMessage(
      &devtools_agent_host_client,
      base::as_bytes(base::make_span(kMsg, strlen(kMsg))));
  reload_observer.Wait();
  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

// This test verifies that a display:none frame loading a MimeHandlerView type
// will end up creating a MimeHandlerview. NOTE: this is an exception to support
// printing in Google docs (see https://crbug.com/978240).
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       MimeHandlerViewInDisplayNoneFrameForGoogleApps) {
  GURL data_url(
      "data:text/html, <iframe src='data:application/pdf,foo' "
      "style='display:none'></iframe>,foo2");
  ui_test_utils::NavigateToURL(browser(), data_url);
  ASSERT_TRUE(GetGuestViewManager()->WaitForSingleGuestCreated());
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, EmbeddedThenPrint) {
  PrintPreviewDelegate print_preview_delegate;
  InitializeTestPage(embedded_test_server()->GetURL("/test_embedded.html"));
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  auto* gv_manager = GetGuestViewManager();
  gv_manager->WaitForAllGuestsDeleted();
  EXPECT_EQ(1U, gv_manager->num_guests_created());

  // Verify that print dialog comes up.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* main_frame = web_contents->GetMainFrame();
  // Use setTimeout() to prevent ExecuteScript() from blocking on the print
  // dialog.
  ASSERT_TRUE(content::ExecuteScript(
      main_frame, "setTimeout(function() { window.print(); }, 0)"));
  print_preview_delegate.WaitUntilPreviewIsReady();
}
#endif
