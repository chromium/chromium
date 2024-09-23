// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/update_user_activation_state_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/extensions_guest_view_manager_delegate.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_stream_manager.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_attach_helper.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_constants.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
#include "chrome/browser/printing/test_print_preview_observer.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#endif

using extensions::ExtensionsAPIClient;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;

namespace {
// The value of the data is "content to read\n".
const char kDataUrlCsv[] = "data:text/csv;base64,Y29udGVudCB0byByZWFkCg==";
}  // namespace

class ChromeMimeHandlerViewTest : public extensions::ExtensionApiTest {
 public:
  ChromeMimeHandlerViewTest() = default;

  ~ChromeMimeHandlerViewTest() override = default;

  void SetUpOnMainThread() override {
    extensions::ExtensionApiTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir_.AppendASCII("mime_handler_view"));
    embedded_test_server()->RegisterRequestMonitor(base::BindRepeating(
        &ChromeMimeHandlerViewTest::MonitorRequest, base::Unretained(this)));
    ASSERT_TRUE(StartEmbeddedTestServer());
  }

 protected:
  TestGuestViewManager* GetGuestViewManager() {
    return factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(),
        ExtensionsAPIClient::Get()->CreateGuestViewManagerDelegate());
  }

  const extensions::Extension* LoadTestExtension() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("mime_handler_view"));
    EXPECT_TRUE(extension);
    EXPECT_EQ(extension_misc::kMimeHandlerPrivateTestExtensionId,
              extension->id());
    return extension;
  }

  void RunTestWithUrl(const GURL& url) {
    // Use the testing subclass of MimeHandlerViewGuest.
    TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());

    const extensions::Extension* extension = LoadTestExtension();
    ASSERT_TRUE(extension);

    extensions::ResultCatcher catcher;

    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    if (!catcher.GetNextResult())
      FAIL() << catcher.message();

    ASSERT_TRUE(GetGuestViewManager()->WaitForSingleGuestViewCreated());
    ASSERT_TRUE(GetEmbedderWebContents());
  }

  void RunTest(const std::string& path) {
    RunTestWithUrl(embedded_test_server()->GetURL("/" + path));
  }

  content::WebContents* GetEmbedderWebContents() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  // In preparation for the migration of guest view from inner WebContents to
  // MPArch (crbug/1261928), individual tests should avoid accessing the guest's
  // inner WebContents. The direct access is centralized in this helper function
  // for easier migration.
  //
  // TODO(crbug.com/40202416): Update this implementation for MPArch, and
  // consider relocate it to `content/public/test/browser_test_utils.h`.
  void WaitForGuestViewLoadStop(GuestViewBase* guest_view) {
    auto* guest_contents = guest_view->web_contents();
    ASSERT_TRUE(content::WaitForLoadStop(guest_contents));
  }

  int basic_count() const { return basic_count_; }

 private:
  void MonitorRequest(const net::test_server::HttpRequest& request) {
    if (request.relative_url == "/testBasic.csv")
      basic_count_++;
  }

  TestGuestViewManagerFactory factory_;
  int basic_count_ = 0;

  ChromeMimeHandlerViewTest(const ChromeMimeHandlerViewTest&) = delete;
  ChromeMimeHandlerViewTest& operator=(const ChromeMimeHandlerViewTest&) =
      delete;
};

namespace {

class UserActivationUpdateWaiter {
 public:
  explicit UserActivationUpdateWaiter(content::RenderFrameHost* rfh)
      : user_activation_interceptor_(rfh) {}
  ~UserActivationUpdateWaiter() = default;

  void Wait() {
    if (user_activation_interceptor_.update_user_activation_state())
      return;
    base::RunLoop run_loop;
    user_activation_interceptor_.set_quit_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  content::UpdateUserActivationStateInterceptor user_activation_interceptor_;
};

// A DevToolsAgentHostClient implementation doing nothing.
class StubDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  StubDevToolsAgentHostClient() {}
  ~StubDevToolsAgentHostClient() override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {}
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, Embedded) {
  RunTest("test_embedded.html");
  // Sanity check. Navigate the page and verify the guest goes away.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  auto* gv_manager = GetGuestViewManager();
  gv_manager->WaitForAllGuestsDeleted();
  EXPECT_EQ(1U, gv_manager->num_guests_created());
}

// This test start with an <object> that has a content frame. Then the content
// frame (plugin frame) is navigated to a cross-origin target page. After the
// navigation is completed, the <object> is set to render MimeHandlerView by
// setting its |data| and |type| attributes accordingly.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       EmbedWithInitialCrossOriginFrame) {
  const std::string kTestName = "test_cross_origin_frame";
  std::string cross_origin_url =
      embedded_test_server()->GetURL("b.com", "/test_page.html").spec();
  auto test_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_object_with_frame.html?test_data=%s,%s,%s",
                         kTestName.c_str(), cross_origin_url.c_str(),
                         "testEmbedded.csv"));
  RunTestWithUrl(test_url);
}

// This test verifies that navigations on the plugin frame before setting it to
// load MimeHandlerView do not race with the creation of the guest. The test
// loads a page with an <object> which is first navigated to some cross-origin
// domain and then immediately after load, the page triggers a navigation of its
// own to another cross-origin domain. Meanwhile the embedder sets the <object>
// to load a MimeHandlerView. The test passes if MHV loads. This is to catch the
// potential race between the cross-origin renderer initiated navigation and
// the navigation to "about:blank" started from the browser.
//
// Disabled on all platforms due to flakiness: https://crbug.com/1182355.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       DISABLED_NavigationRaceFromEmbedder) {
  const std::string kTestName = "test_navigation_race_embedder";
  auto cross_origin_url =
      embedded_test_server()->GetURL("b.com", "/test_page.html").spec();
  auto test_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_object_with_frame.html?test_data=%s,%s,%s",
                         kTestName.c_str(), cross_origin_url.c_str(),
                         "testEmbedded.csv"));
  RunTestWithUrl(test_url);
}

// TODO(ekaramad): Without proper handling of navigation to 'about:blank', this
// test would be flaky. Use TestNavigationManager class and possibly break the
// test into more sub-tests for various scenarios (https://crbug.com/659750).
// This test verifies that (almost) concurrent navigations in a cross-process
// frame inside an <embed> which is transitioning to a MimeHandlerView will
// not block creation of MimeHandlerView. The test will load some cross-origin
// content in <object> which right after loading will navigate it self to some
// other cross-origin content. On the embedder side, when the first page loads,
// the <object> loads some text/csv content to create a MimeHandlerViewGuest.
// The test passes if MHV loads.
// TODO(crbug.com/40751404): Disabled due to flakes.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       DISABLED_NavigationRaceFromCrossProcessRenderer) {
  const std::string kTestName = "test_navigation_race_cross_origin";
  auto cross_origin_url =
      embedded_test_server()->GetURL("b.com", "/test_page.html").spec();
  auto other_cross_origin_url =
      embedded_test_server()->GetURL("c.com", "/test_page.html").spec();
  auto test_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_object_with_frame.html?test_data=%s,%s,%s,%s",
                         kTestName.c_str(), cross_origin_url.c_str(),
                         other_cross_origin_url.c_str(), "testEmbedded.csv"));
  RunTestWithUrl(test_url);
}

// This test verifies that removing embedder RenderFrame will not crash the
// renderer (for context see https://crbug.com/930803).
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, EmbedderFrameRemovedNoCrash) {
  RunTest("test_iframe_basic.html");
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  int32_t element_instance_id = guest_view->element_instance_id();
  auto* embedder_web_contents = GetEmbedderWebContents();
  auto* child_frame =
      content::ChildFrameAt(embedder_web_contents->GetPrimaryMainFrame(), 0);
  content::RenderFrameDeletedObserver render_frame_observer(child_frame);
  ASSERT_TRUE(
      content::ExecJs(embedder_web_contents,
                      "document.querySelector('iframe').outerHTML = ''"));
  render_frame_observer.WaitUntilDeleted();
  // Send the IPC. During destruction MHVFC would cause a UaF since it was not
  // removed from the global map.
  mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
      container_manager;
  embedder_web_contents->GetPrimaryMainFrame()
      ->GetRemoteAssociatedInterfaces()
      ->GetInterface(&container_manager);
  container_manager->DestroyFrameContainer(element_instance_id);
  // Running the following JS code fails if the renderer has crashed.
  ASSERT_TRUE(content::ExecJs(embedder_web_contents, "window.name = 'foo'"));
}

// TODO(ekaramad): Somehow canceling a first dialog in a setup similar to the
// test below pops up another dialog. This is likely due to the navigation to
// about:blank from both the browser side and the embedder side in the method
// HTMLPlugInElement::RequestObjectInternal. Find out the issue and add another
// test here where the dialog is dismissed and the guest not created.
// (https://crbug.com/659750).
// This test verifies that transitioning a plugin element from text/html to
// application/pdf respects 'beforeunload'. The test specifically checks that
// 'beforeunload' dialog is shown to the user and if the user decides to
// proceed with the transition, MimeHandlerViewGuest is created.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       EmbedWithInitialFrameAcceptBeforeUnloadDialog) {
  // Use the testing subclass of MimeHandlerViewGuest.
  TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());
  const extensions::Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("a.com", "/test_object_with_frame.html")));
  auto* main_frame =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetPrimaryMainFrame();
  auto url_with_beforeunload =
      embedded_test_server()->GetURL("b.com", "/test_page.html?beforeunload");
  ASSERT_EQ(true, content::EvalJs(main_frame,
                                  base::StringPrintf(
                                      "object.data = '%s';"
                                      "new Promise(resolve => {"
                                      "  object.onload = () => resolve(true);"
                                      "});",
                                      url_with_beforeunload.spec().c_str())));
  // Give user gesture to the frame, set the <object> to text/csv resource and
  // handle the "beforeunload" dialog.
  content::PrepContentsForBeforeUnloadTest(
      browser()->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(content::ExecJs(main_frame,
                              "object.data = './testEmbedded.csv';"
                              "object.type = 'text/csv';"));
  javascript_dialogs::AppModalDialogController* alert =
      ui_test_utils::WaitForAppModalDialog();
  ASSERT_TRUE(alert->is_before_unload_dialog());
  alert->view()->AcceptAppModalDialog();

  EXPECT_TRUE(GetGuestViewManager()->WaitForSingleGuestViewCreated());
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, PostMessage) {
  RunTest("test_postmessage.html");
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, Basic) {
  RunTest("testBasic.csv");
  // Verify that for a navigation to a MimeHandlerView MIME type, exactly one
  // stream is intercepted. This means we do not create a PluginDocument. If a
  // PluginDocument was created here, the |view_id| associated with the
  // stream intercepted from navigation response would be lost (
  // PluginDocument does not talk to a MimeHandlerViewFrameContainer). Then,
  // the newly added <embed> by the PluginDocument would send its own request
  // leading to a total of 2 intercepted streams. The first one (from
  // navigation) would never be released.
  EXPECT_EQ(0U, extensions::MimeHandlerStreamManager::Get(
                    GetEmbedderWebContents()->GetBrowserContext())
                    ->streams_.size());
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, Iframe) {
  RunTest("test_iframe.html");
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, NonAsciiHeaders) {
  RunTest("testNonAsciiHeaders.csv");
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, DataUrl) {
  RunTestWithUrl(GURL(kDataUrlCsv));
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, EmbeddedDataUrlObject) {
  RunTest("test_embedded_data_url_object.html");
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, EmbeddedDataUrlEmbed) {
  RunTest("test_embedded_data_url_embed.html");
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, EmbeddedDataUrlLong) {
  RunTest("test_embedded_data_url_long.html");
}

// Regression test for crbug.com/587709.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, SingleRequest) {
  GURL url(embedded_test_server()->GetURL("/testBasic.csv"));
  RunTest("testBasic.csv");
  EXPECT_EQ(1, basic_count());
}

// Test that a mime handler view can keep a background page alive.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, BackgroundPage) {
  extensions::ProcessManager::SetEventPageIdleTimeForTesting(1);
  extensions::ProcessManager::SetEventPageSuspendingTimeForTesting(1);
  RunTest("testBackgroundPage.csv");
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, TargetBlankAnchor) {
  RunTest("testTargetBlankAnchor.csv");
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetWebContentsAt(1)));
  EXPECT_EQ(
      GURL(url::kAboutBlankURL),
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, BeforeUnload_NoDialog) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadNoDialog.csv"));
  auto* web_contents = GetEmbedderWebContents();
  content::PrepContentsForBeforeUnloadTest(web_contents);

  // Wait for a round trip to the outer renderer to ensure any beforeunload
  // toggle IPC has had time to reach the browser.
  ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(), ""));

  // Try to navigate away from the page. If the beforeunload listener is
  // triggered and a dialog is shown, this navigation will never complete,
  // causing the test to timeout and fail.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, BeforeUnload_ShowDialog) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadShowDialog.csv"));
  auto* web_contents = GetEmbedderWebContents();
  content::PrepContentsForBeforeUnloadTest(web_contents);

  // Wait for a round trip to the outer renderer to ensure the beforeunload
  // toggle IPC has had time to reach the browser.
  ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(), ""));

  web_contents->GetController().LoadURL(GURL(url::kAboutBlankURL), {},
                                        ui::PAGE_TRANSITION_TYPED, "");

  javascript_dialogs::AppModalDialogController* before_unload_dialog =
      ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(before_unload_dialog->is_before_unload_dialog());
  EXPECT_FALSE(before_unload_dialog->is_reload());
  before_unload_dialog->OnAccept(std::u16string(), false);
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       BeforeUnloadEnabled_WithoutUserActivation) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadWithUserActivation.csv"));
  auto* web_contents = GetEmbedderWebContents();
  // Prepare frames but don't trigger user activation.
  content::PrepContentsForBeforeUnloadTest(web_contents, false);

  // Even though this test's JS setup enables BeforeUnload dialogs, the dialog
  // is still suppressed here because of lack of user activation.  As a result,
  // the following navigation away from the page works fine.  If a beforeunload
  // dialog were shown, this navigation would fail, causing the test to timeout.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       BeforeUnloadEnabled_WithUserActivation) {
  ASSERT_NO_FATAL_FAILURE(RunTest("testBeforeUnloadWithUserActivation.csv"));
  auto* web_contents = GetEmbedderWebContents();
  // Prepare frames but don't trigger user activation across all frames.
  content::PrepContentsForBeforeUnloadTest(web_contents, false);

  // Make sure we have a guestviewmanager.
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);

  UserActivationUpdateWaiter activation_waiter(guest_view->GetGuestMainFrame());

  // Activate |guest_view| through a click, then wait until the activation IPC
  // reaches the browser process.
  content::WaitForHitTestData(guest_view->GetGuestMainFrame());
  SimulateMouseClickAt(web_contents, 0, blink::WebMouseEvent::Button::kLeft,
                       guest_view->GetGuestMainFrame()
                           ->GetView()
                           ->TransformPointToRootCoordSpace(gfx::Point(5, 5)));
  activation_waiter.Wait();

  // Wait for a round trip to the outer renderer to ensure any beforeunload
  // toggle IPC has had time to reach the browser.
  ASSERT_TRUE(content::ExecJs(web_contents->GetPrimaryMainFrame(), ""));

  // Try to navigate away, this should invoke a beforeunload dialog.
  web_contents->GetController().LoadURL(GURL(url::kAboutBlankURL), {},
                                        ui::PAGE_TRANSITION_TYPED, "");

  javascript_dialogs::AppModalDialogController* before_unload_dialog =
      ui_test_utils::WaitForAppModalDialog();
  EXPECT_TRUE(before_unload_dialog->is_before_unload_dialog());
  EXPECT_FALSE(before_unload_dialog->is_reload());
  before_unload_dialog->OnAccept(std::u16string(), false);
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       ActivatePostMessageSupportOnce) {
  RunTest("test_embedded.html");
  // Attach a second <embed>.
  ASSERT_TRUE(content::ExecJs(GetEmbedderWebContents(),
                              "const e = document.createElement('embed');"
                              "e.src = './testEmbedded.csv'; e.type='text/csv';"
                              "document.body.appendChild(e);"));

  auto* guest_view = GetGuestViewManager()->WaitForNextGuestViewCreated();
  ASSERT_TRUE(guest_view);
  WaitForGuestViewLoadStop(guest_view);

  // After load, an IPC has been sent to the renderer to update routing IDs for
  // the guest frame and the content frame (and activate the
  // PostMessageSupport). Run some JS to Ensure no DCHECKs have fired in the
  // embedder process.
  ASSERT_TRUE(content::ExecJs(GetEmbedderWebContents(), "foo = 0;"));
}

// This is a minimized repro for a clusterfuzz crasher and is not really related
// to MimeHandlerView. The test verifies that when
// HTMLPlugInElement::PluginWrapper is called for a plugin with no node document
// frame, the renderer does not crash (see https://966371).
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       AdoptNodeInOnLoadDoesNotCrash) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      embedded_test_server()->GetURL("/adopt_node_in_onload_no_crash.html")));
  // Run some JavaScript in embedder and make sure it is not crashed.
  ASSERT_TRUE(content::ExecJs(GetEmbedderWebContents(), "true"));
}

// Verifies that sandboxed frames do not create GuestViews (plugins are
// blocked in sandboxed frames).
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, DoNotLoadInSandboxedFrame) {
  // Use the testing subclass of MimeHandlerViewGuest.
  TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());

  const extensions::Extension* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/test_sandboxed_frame.html")));

  auto* guest_view_manager = GetGuestViewManager();
  // The page contains three <iframes> where two are sandboxed. The expectation
  // is that the sandboxed frames do not end up creating a MimeHandlerView.
  // Therefore, it suffices to wait for one GuestView to be created, then remove
  // the non-sandboxed frame, and ensue there are no GuestViews left.
  if (guest_view_manager->num_guests_created() == 0)
    ASSERT_TRUE(GetGuestViewManager()->WaitForNextGuestViewCreated());
  ASSERT_EQ(1U, guest_view_manager->num_guests_created());

  // Remove the non-sandboxed frame.
  content::RenderFrameHost* main_rfh =
      GetEmbedderWebContents()->GetPrimaryMainFrame();
  ASSERT_TRUE(content::ExecJs(main_rfh, "remove_frame('notsandboxed');"));
  // The page is expected to embed only '1' GuestView. If there is GuestViews
  // embedded inside other frames we should be timing out here.
  guest_view_manager->WaitForAllGuestsDeleted();

  // Since 'sandbox1' has no fallback content, we would render an error page in
  // the iframe. Note that we can't access the contentDocument because error
  // pages have opaque origins (so it's using a different origin than the main
  // frame).
  EXPECT_EQ(false, content::EvalJs(main_rfh, "!!(sandbox1.contentDocument)"));
  // The error page will not be blank.
  EXPECT_EQ(true,
            content::EvalJs(ChildFrameAt(main_rfh, 0),
                            "!!(document.body && document.body.firstChild)"));

  // The document inside 'sandbox2' contains an <object> with fallback content.
  // The expectation is that the <object> fails to load the MimeHandlerView and
  // should show the fallback content instead.
  EXPECT_EQ(true, content::EvalJs(main_rfh, "!!(sandbox2.contentDocument)"));
  EXPECT_EQ(
      "Fallback",
      content::EvalJs(
          main_rfh,
          "sandbox2.contentDocument.getElementById('fallback').innerText"));
}

// Tests that a MimeHandlerViewGuest auto-rejects pointer lock requests.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, RejectPointLock) {
  TestMimeHandlerViewGuest::RegisterTestGuestViewType(GetGuestViewManager());

  auto* extension = LoadTestExtension();
  ASSERT_TRUE(extension);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/test_embedded.html")));

  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  auto* guest_rfh = guest_view->GetGuestMainFrame();
  EXPECT_EQ(false, content::EvalJs(guest_rfh, R"code(
    var promise = new Promise((resolve, reject) => {
      document.addEventListener('pointerlockchange', () => resolve(true));
      document.addEventListener('pointerlockerror', () => resolve(false));
    });
    document.body.requestPointerLock();
    (async ()=> { return await promise; })();
  )code",
                                   content::EXECUTE_SCRIPT_DEFAULT_OPTIONS,
                                   1 /* world_id */));
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       GuestDevToolsReloadsEmbedder) {
  GURL data_url(kDataUrlCsv);
  RunTestWithUrl(data_url);
  auto* embedder_web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  EXPECT_NE(embedder_web_contents->GetPrimaryMainFrame(),
            guest_view->GetGuestMainFrame());
  TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(guest_view);

  // Load DevTools.
  scoped_refptr<content::DevToolsAgentHost> devtools_agent_host =
      content::DevToolsAgentHost::GetOrCreateFor(guest_view->web_contents());
  StubDevToolsAgentHostClient devtools_agent_host_client;
  devtools_agent_host->AttachClient(&devtools_agent_host_client);

  // Reload via guest's DevTools, embedder should reload.
  content::TestNavigationObserver reload_observer(embedder_web_contents);
  constexpr char kMsg[] = R"({"id":1,"method":"Page.reload"})";
  devtools_agent_host->DispatchProtocolMessage(
      &devtools_agent_host_client, base::byte_span_from_cstring(kMsg));
  reload_observer.Wait();
  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

// This test verifies that a display:none frame loading a MimeHandlerView type
// will end up creating a MimeHandlerview. NOTE: this is an exception to support
// printing in Google docs (see https://crbug.com/978240).
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest,
                       MimeHandlerViewInDisplayNoneFrameForGoogleApps) {
  GURL data_url(
      base::StringPrintf("data:text/html, <iframe src='%s' "
                         "style='display:none'></iframe>,foo2",
                         kDataUrlCsv));
  RunTestWithUrl(data_url);
}

#if BUILDFLAG(ENABLE_PRINT_PREVIEW)
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, EmbeddedThenPrint) {
  printing::TestPrintPreviewObserver print_observer(/*wait_for_loaded=*/false);
  RunTestWithUrl(embedded_test_server()->GetURL("/test_embedded.html"));
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  auto* gv_manager = GetGuestViewManager();
  gv_manager->WaitForAllGuestsDeleted();
  EXPECT_EQ(1U, gv_manager->num_guests_created());

  // Verify that print dialog comes up.
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* main_frame = web_contents->GetPrimaryMainFrame();
  // Use setTimeout() to prevent ExecJs() from blocking on the print
  // dialog.
  ASSERT_TRUE(content::ExecJs(main_frame,
                              "setTimeout(function() { window.print(); }, 0)"));
  print_observer.WaitUntilPreviewIsReady();
}
#endif

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewTest, FrameIterationBeforeAttach) {
  TestGuestViewManager* manager = GetGuestViewManager();
  TestMimeHandlerViewGuest::RegisterTestGuestViewType(manager);
  ASSERT_TRUE(LoadTestExtension());
  const GURL initial_url(embedded_test_server()->GetURL("/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Navigate to a page with a MimeHandlerView inside an iframe and pause
  // between the creation of the MimeHandlerView and when it is attached to the
  // placeholder frame inside the iframe.
  base::RunLoop pre_attach_run_loop;
  base::OnceClosure resume_attach;
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* mime_handler_view_helper = extensions::MimeHandlerViewAttachHelper::Get(
      web_contents->GetPrimaryMainFrame()->GetProcess()->GetID());
  mime_handler_view_helper->set_resume_attach_callback_for_testing(
      base::BindLambdaForTesting([&](base::OnceClosure resume_closure) {
        resume_attach = std::move(resume_closure);
        pre_attach_run_loop.Quit();
      }));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/test_iframe.html")));
  pre_attach_run_loop.Run();

  auto* guest = manager->GetLastGuestViewCreated();
  auto* guest_main_frame = guest->GetGuestMainFrame();
  content::RenderFrameHost* expected_outermost_rfh =
      web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* expected_embedder_rfh =
      content::ChildFrameAt(expected_outermost_rfh, 0);

  // Ensure that iterating through ancestors is correct even when pending
  // attachment.
  EXPECT_EQ(nullptr, guest_main_frame->GetParent());
  EXPECT_EQ(guest_main_frame, guest_main_frame->GetMainFrame());
  EXPECT_EQ(nullptr, guest_main_frame->GetParentOrOuterDocument());
  EXPECT_EQ(guest_main_frame, guest_main_frame->GetOutermostMainFrame());
  // In particular, MimeHandlerView should still be considered to have an
  // embedder in this state.
  EXPECT_EQ(expected_embedder_rfh,
            guest_main_frame->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(expected_outermost_rfh,
            guest_main_frame->GetOutermostMainFrameOrEmbedder());

  // Complete attachment and ensure the results afterwords are consistent with
  // the above.
  std::move(resume_attach).Run();
  manager->WaitUntilAttached(guest);

  EXPECT_EQ(nullptr, guest_main_frame->GetParent());
  EXPECT_EQ(guest_main_frame, guest_main_frame->GetMainFrame());
  EXPECT_EQ(nullptr, guest_main_frame->GetParentOrOuterDocument());
  EXPECT_EQ(guest_main_frame, guest_main_frame->GetOutermostMainFrame());
  EXPECT_EQ(expected_embedder_rfh,
            guest_main_frame->GetParentOrOuterDocumentOrEmbedder());
  EXPECT_EQ(expected_outermost_rfh,
            guest_main_frame->GetOutermostMainFrameOrEmbedder());
}
