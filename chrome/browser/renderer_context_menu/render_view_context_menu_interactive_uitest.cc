// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/custom_handlers/protocol_handler_registry_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tab_contents/chrome_web_contents_view_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom-shared.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/launchservices_utils_mac.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/custom_handlers/protocol_handler.h"
#include "components/custom_handlers/protocol_handler_registry.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/privacy_sandbox_attestations.h"
#include "components/privacy_sandbox/privacy_sandbox_attestations/scoped_privacy_sandbox_attestations.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/gfx/geometry/point_f.h"
#include "url/gurl.h"
#include "url/origin.h"

using testing::AllOf;
using testing::Contains;
using testing::Ge;
using testing::IsSupersetOf;
using testing::Le;
using testing::Not;

namespace {

class ContextMenuUiTest : public InteractiveBrowserTest {
 public:
  ContextMenuUiTest() = default;

  ContextMenuUiTest(const ContextMenuUiTest&) = delete;
  ContextMenuUiTest& operator=(const ContextMenuUiTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// This is a regression test for https://crbug.com/1257907.  It tests using
// "Open link in new tab" context menu item in a subframe, to follow a link
// that should stay in the same SiteInstance (e.g. "about:blank", or "data:"
// URL).  This test is somewhat similar to ChromeNavigationBrowserTest's
// ContextMenuNavigationToInvalidUrl testcase, but 1) uses a subframe, and 2)
// more accurately simulates what the product code does.
//
// The test is compiled out on Mac, because RenderViewContextMenuMacCocoa::Show
// requires running a nested message loop - this would undesirably yield control
// over the next steps to the OS.
#if !BUILDFLAG(IS_MAC)
IN_PROC_BROWSER_TEST_F(ContextMenuUiTest,
                       ContextMenuNavigationToAboutBlankUrlInSubframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to a page with a cross-site subframe.
  GURL start_url = embedded_test_server()->GetURL(
      "start.com", "/frame_tree/page_with_two_frames_remote_and_local.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), start_url));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* subframe = content::ChildFrameAt(main_frame, 0);
  ASSERT_NE(main_frame->GetLastCommittedOrigin(),
            subframe->GetLastCommittedOrigin());
  ASSERT_EQ("bar.com", subframe->GetLastCommittedOrigin().host());

  // Prepare ContextMenuParams that correspond to a link to an about:blank URL
  // in the cross-site subframe.  This preparation to some extent
  // duplicates/replicates the code in RenderFrameHostImpl::ShowContextMenu.
  //
  // Note that the repro steps in https://crbug.com/1257907 resulted in a
  // navigation to about:blank#blocked because of how a navigation to
  // javascript: URL gets rewritten by RenderProcessHost::FilterURL calls.
  // Directly navigating to an about:blank URL is just as good for replicating a
  // discrepancy between `source_site_instance` and `initiator_origin` (without
  // relying on implementation details of FilterURL).
  GURL link_url("about:blank#blah");
  content::ContextMenuParams params;
  params.link_url = link_url;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = main_frame->GetLastCommittedURL();
  params.frame_url = subframe->GetLastCommittedURL();
  content::RenderProcessHost* process = subframe->GetProcess();
  process->FilterURL(true, &params.link_url);
  process->FilterURL(true, &params.src_url);

  // Simulate opening a context menu.
  //
  // Note that we can't use TestRenderViewContextMenu (like some other tests),
  // because this wouldn't exercise the product code responsible for the
  // https://crbug.com/1257907 bug (it wouldn't go through
  // ChromeWebContentsViewDelegateViews::ShowContextMenu).
  std::unique_ptr<content::WebContentsViewDelegate> view_delegate =
      CreateWebContentsViewDelegate(web_contents);
  view_delegate->ShowContextMenu(*subframe, params);

  // Simulate using the context menu to "Open link in new tab".
  content::WebContents* new_web_contents = nullptr;
  {
    ui_test_utils::TabAddedWaiter tab_add(browser());
    view_delegate->ExecuteCommandForTesting(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB,
                                            0);
    tab_add.Wait();
    int index_of_new_tab = browser()->tab_strip_model()->count() - 1;
    new_web_contents =
        browser()->tab_strip_model()->GetWebContentsAt(index_of_new_tab);
  }

  // Verify that the load succeeded and was associated with the right
  // SiteInstance.
  EXPECT_TRUE(WaitForLoadStop(new_web_contents));
  EXPECT_EQ(link_url, new_web_contents->GetLastCommittedURL());
  EXPECT_EQ(new_web_contents->GetPrimaryMainFrame()->GetSiteInstance(),
            subframe->GetSiteInstance());
}
#endif  // !BUILDFLAG(IS_MAC)

// TODO(crbug.com/375048798): Once Kombucha framework supports querying elements
// inside iframe and fenced frame, convert the context menu tests to use
// Kombucha framework.
class ContextMenuFencedFrameTest : public ContextMenuUiTest {
 public:
  ContextMenuFencedFrameTest() = default;
  ~ContextMenuFencedFrameTest() override = default;
  ContextMenuFencedFrameTest(const ContextMenuFencedFrameTest&) = delete;
  ContextMenuFencedFrameTest& operator=(const ContextMenuFencedFrameTest&) =
      delete;

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() override {
    ContextMenuUiTest::SetUpOnMainThread();

    embedded_https_test_server().AddDefaultHandlers(GetChromeTestDataDir());
    // Add content/test/data for cross_site_iframe_factory.html.
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        "content/test/data");
    embedded_https_test_server().SetSSLConfig(
        net::EmbeddedTestServer::CERT_TEST_NAMES);
  }

  // Create a fenced frame which is navigated to `url`.
  content::RenderFrameHost* CreateFencedFrame(const GURL& url) {
    // Navigate fenced frame to a page with an anchor element.
    GURL main_url(embedded_https_test_server().GetURL(
        "a.test",
        base::StringPrintf("/cross_site_iframe_factory.html?a.test(%s{fenced})",
                           url.spec().c_str())));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Get fenced frame render frame host.
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(
            primary_main_frame_host());
    EXPECT_EQ(child_frames.size(), 1u);
    content::RenderFrameHost* fenced_frame_rfh = child_frames[0];
    EXPECT_EQ(fenced_frame_rfh->GetLastCommittedURL(), url);

    return fenced_frame_rfh;
  }

  // Create a fenced frame with a nested iframe. The nested iframe is navigated
  // to `url`.
  content::RenderFrameHost* CreateFencedFrameWithNestedIframe(const GURL& url) {
    // The page has a fenced frame with a nested iframe inside.
    GURL main_url(embedded_https_test_server().GetURL(
        "a.test",
        base::StringPrintf(
            "/cross_site_iframe_factory.html?a.test(a.test{fenced}(%s))",
            url.spec().c_str())));
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

    // Get fenced frame render frame host.
    std::vector<content::RenderFrameHost*> child_frames =
        fenced_frame_test_helper().GetChildFencedFrameHosts(
            primary_main_frame_host());
    EXPECT_EQ(child_frames.size(), 1u);
    content::RenderFrameHost* fenced_frame_rfh = child_frames[0];

    return fenced_frame_rfh;
  }

  content::RenderFrameHost* primary_main_frame_host() {
    return browser()
        ->tab_strip_model()
        ->GetActiveWebContents()
        ->GetPrimaryMainFrame();
  }

  void InstallTestWebApp(const GURL& start_url) {
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope = start_url;
    web_app_info->title = u"Test app";
    web_app_info->description = u"Test description";
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;

    web_app::test::InstallWebApp(browser()->profile(), std::move(web_app_info));
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Check which commands are present after opening the context menu for a
// fencedframe.
// TODO(crbug.com/40273673): Enable the test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_MenuContentsVerification_Fencedframe \
  DISABLED_MenuContentsVerification_Fencedframe
#else
#define MAYBE_MenuContentsVerification_Fencedframe \
  MenuContentsVerification_Fencedframe
#endif
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       MAYBE_MenuContentsVerification_Fencedframe) {
  ASSERT_TRUE(embedded_test_server()->Start());

  auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(
      embedded_test_server()->GetURL("/fenced_frames/title1.html"));
  auto* fenced_frame_rfh = fenced_frame_test_helper().CreateFencedFrame(
      primary_main_frame_host(), fenced_frame_url);

  // To avoid a flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Open a context menu.
  ContextMenuWaiter menu_observer;
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kRight;
  // These coordinates are relative to the fenced frame's widget since we're
  // forwarding this event directly to the widget.
  mouse_event.SetPositionInWidget(50, 50);

  auto* fenced_frame_widget = fenced_frame_rfh->GetRenderWidgetHost();
  fenced_frame_widget->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  fenced_frame_widget->ForwardMouseEvent(mouse_event);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              IsSupersetOf({IDC_BACK, IDC_FORWARD, IDC_RELOAD, IDC_VIEW_SOURCE,
                            IDC_SAVE_PAGE, IDC_CONTENT_CONTEXT_VIEWFRAMESOURCE,
                            IDC_CONTENT_CONTEXT_RELOADFRAME,
                            IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveLinkAsEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with an anchor element.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/download-anchor-same-origin.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Link As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Link As..." should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVELINKAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveLinkAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with an anchor element.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/download-anchor-same-origin.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Link As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Link As..." should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVELINKAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVELINKAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveAudioAsEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with an audio element.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/accessibility/html/audio.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Click the audio element inside the fenced frame.
  const gfx::PointF audio_element(15, 15);

  // Open a context menu by right clicking on the audio element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveAudioAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with an audio element.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/accessibility/html/audio.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Click the audio element inside the fenced frame.
  gfx::PointF audio_element(15, 15);

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the click point needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  audio_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the audio element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, audio_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Audio As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEAVAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveVideoAsEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with a video element.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/media/video-player-autoplay.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForLoadStop(
      content::WebContents::FromRenderFrameHost(fenced_frame_rfh));

  // Click the video inside the fenced frame.
  const gfx::PointF click_point(15, 15);

  // Open a context menu by right clicking on the video element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be present and
  // enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                            IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                            IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be disabled in the
  // context menu after fenced frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                            IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveVideoAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with a video element.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/media/video-player-autoplay.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Click the video inside the fenced frame.
  gfx::PointF click_point(15, 15);

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the click point needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  click_point.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the video element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be present and
  // enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                            IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                            IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Video As..." and "Save Video Frame As..." should be disabled in the
  // context menu after fenced frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              IsSupersetOf({IDC_CONTENT_CONTEXT_SAVEAVAS,
                            IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS}));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEAVAS)));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEVIDEOFRAMEAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       SaveImageAsEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to an image.
  GURL fenced_frame_url(
      embedded_https_test_server().GetURL("a.test", "/test_visual.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Click inside the fenced frame.
  const gfx::PointF click_point(15, 15);

  // Open a context menu by right clicking on the image.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Image As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Image As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    SaveImageAsEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to an image.
  GURL nested_iframe_url(
      embedded_https_test_server().GetURL("a.test", "/test_visual.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh are
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Click inside the nested iframe.
  gfx::PointF click_point(15, 15);

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the click point needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  click_point.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the image.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Image As..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, click_point);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Image As..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_SAVEIMAGEAS)));
}

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTest,
                       OpenLinkInWebAppEntryIsDisabledAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      "content/test/data");
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with an anchor element with href to a web
  // App.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/web_app.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // Install the anchor element href as a web App.
  InstallTestWebApp(GURL("https://www.google.com/"));

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Open Link In [App]" should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Open Link In [App]" should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  EXPECT_THAT(
      menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameTest,
    OpenLinkInWebAppEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  // Add content/test/data for cross_site_iframe_factory.html.
  embedded_https_test_server().ServeFilesFromSourceDirectory(
      "content/test/data");
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with an anchor element with href to a
  // web App.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/web_app.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // Install the anchor element href as a web App.
  InstallTestWebApp(GURL("https://www.google.com/"));

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Open Link In [App]" should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Open Link In [App]" should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP));
  EXPECT_THAT(
      menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
      testing::Not(testing::Contains(IDC_CONTENT_CONTEXT_OPENLINKBOOKMARKAPP)));
}

// "Open Link in Profile" functionality is not available on ChromeOS Ash where
// there is only one profile.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ContextMenuFencedFrameMutilpleProfilesTest
    : public ContextMenuFencedFrameTest {
 public:
  ContextMenuFencedFrameMutilpleProfilesTest() = default;
  ~ContextMenuFencedFrameMutilpleProfilesTest() override = default;

  void SetUpOnMainThread() override {
    ContextMenuFencedFrameTest::SetUpOnMainThread();

    ProfileManager* profile_manager = g_browser_process->profile_manager();
    Profile& secondary_profile = profiles::testing::CreateProfileSync(
        profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
    CreateBrowser(&secondary_profile);
  }
};

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameMutilpleProfilesTest,
                       OpenLinkInProfileEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with an anchor element.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/download-anchor-same-origin.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // No sub-menu is shown because there is only on other active profile.
  ASSERT_THAT(menu_observer.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE)));
  // "Open Link as User ..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                             Le(IDC_OPEN_LINK_IN_PROFILE_LAST))));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                             Le(IDC_OPEN_LINK_IN_PROFILE_LAST))));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // No sub-menu is shown.
  ASSERT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE)));
  // "Open Link in Profile" should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                             Le(IDC_OPEN_LINK_IN_PROFILE_LAST))));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                                 Le(IDC_OPEN_LINK_IN_PROFILE_LAST)))));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameMutilpleProfilesTest,
    OpenLinkInProfileEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with an anchor element.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/download-anchor-same-origin.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // No sub-menu is shown because there is only on other active profile.
  ASSERT_THAT(menu_observer.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE)));
  // "Open Link as User ..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                             Le(IDC_OPEN_LINK_IN_PROFILE_LAST))));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                             Le(IDC_OPEN_LINK_IN_PROFILE_LAST))));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // No sub-menu is shown.
  ASSERT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKINPROFILE)));
  // "Open Link in Profile" should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                             Le(IDC_OPEN_LINK_IN_PROFILE_LAST))));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(AllOf(Ge(IDC_OPEN_LINK_IN_PROFILE_FIRST),
                                 Le(IDC_OPEN_LINK_IN_PROFILE_LAST)))));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

class ContextMenuFencedFrameProtocolHandlerTest
    : public ContextMenuFencedFrameTest {
 public:
  ContextMenuFencedFrameProtocolHandlerTest() = default;
  ~ContextMenuFencedFrameProtocolHandlerTest() override = default;

  void SetUpOnMainThread() override {
    ContextMenuFencedFrameTest::SetUpOnMainThread();

#if BUILDFLAG(IS_MAC)
    ASSERT_TRUE(test::RegisterAppWithLaunchServices());
#endif

    // Add a protocol handler.
    std::string protocol{"web+search"};
    AddProtocolHandler(protocol, GURL("https://www.google.com/%s"));
    custom_handlers::ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    ASSERT_EQ(1u, registry->GetHandlersFor(protocol).size());
  }

  void AddProtocolHandler(const std::string& protocol, const GURL& url) {
    custom_handlers::ProtocolHandler handler =
        custom_handlers::ProtocolHandler::CreateProtocolHandler(protocol, url);
    custom_handlers::ProtocolHandlerRegistry* registry =
        ProtocolHandlerRegistryFactory::GetForBrowserContext(
            browser()->profile());
    // Fake that this registration is happening on profile startup. Otherwise
    // it'll try to register with the OS, which causes DCHECKs on Windows when
    // running as admin on Windows 7.
    registry->SetIsLoading(true);
    registry->OnAcceptRegisterProtocolHandler(handler);
    registry->SetIsLoading(true);
    ASSERT_TRUE(registry->IsHandledProtocol(protocol));
  }
};

IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameProtocolHandlerTest,
                       OpenLinkWithEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with an anchor element, whose href will be
  // handled by a custom protocol handler.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/protocol_handler.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Open Link With..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Open Link With..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuFencedFrameProtocolHandlerTest,
    OpenLinkWithEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with an anchor element, whose href
  // will be handled by a custom protocol handler.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/protocol_handler.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Save Link With..." should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Save Link With..." should be disabled in the context menu after fenced
  // frame has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKWITH)));
}

class ContextMenuLinkPreviewFencedFrameTest
    : public ContextMenuFencedFrameTest {
 public:
  ContextMenuLinkPreviewFencedFrameTest() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kLinkPreview);
  }

  ~ContextMenuLinkPreviewFencedFrameTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextMenuLinkPreviewFencedFrameTest,
                       LinkPreviewEntryIsDisabledAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate fenced frame to a page with an anchor element.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/download-anchor-same-origin.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrame(fenced_frame_url);

  // To avoid flakiness and ensure fenced_frame_rfh is ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);

  // Get the coordinate of the anchor element inside the fenced frame.
  const gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(fenced_frame_rfh, "anchor");

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Preview Link" should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      fenced_frame_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Preview Link" should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW)));
}

IN_PROC_BROWSER_TEST_F(
    ContextMenuLinkPreviewFencedFrameTest,
    LinkPreviewEntryIsDisabledInNestedIframeAfterNetworkCutoff) {
  ASSERT_TRUE(embedded_https_test_server().Start());

  // Navigate the nested iframe to a page with an anchor element.
  GURL nested_iframe_url(embedded_https_test_server().GetURL(
      "a.test", "/download-anchor-same-origin.html"));

  content::RenderFrameHost* fenced_frame_rfh =
      CreateFencedFrameWithNestedIframe(nested_iframe_url);
  content::RenderFrameHost* nested_iframe_rfh =
      content::ChildFrameAt(fenced_frame_rfh, 0);
  ASSERT_EQ(nested_iframe_rfh->GetLastCommittedURL(), nested_iframe_url);

  // To avoid flakiness and ensure fenced_frame_rfh and nested_iframe_rfh is
  // ready for hit testing.
  content::WaitForHitTestData(fenced_frame_rfh);
  content::WaitForHitTestData(nested_iframe_rfh);

  // Get the coordinate of the anchor element inside the nested iframe.
  gfx::PointF anchor_element =
      GetCenterCoordinatesOfElementWithId(nested_iframe_rfh, "anchor");

  // Because the mouse event is forwarded to the `RenderWidgetHost` of the
  // fenced frame, the anchor element needs to be offset by the top left
  // coordinates of the nested iframe relative to the fenced frame.
  const gfx::PointF iframe_offset =
      content::test::GetTopLeftCoordinatesOfElementWithId(fenced_frame_rfh,
                                                          "child-0");
  anchor_element.Offset(iframe_offset.x(), iframe_offset.y());

  // Open a context menu by right clicking on the anchor element.
  ContextMenuWaiter menu_observer;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer.WaitForMenuOpenAndClose();

  // "Preview Link" should be present and enabled in the context menu.
  EXPECT_THAT(menu_observer.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(menu_observer.GetCapturedEnabledCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));

  // Disable fenced frame untrusted network access.
  ASSERT_TRUE(ExecJs(fenced_frame_rfh, R"(
    (async () => {
      return window.fence.disableUntrustedNetwork();
    })();
  )"));

  // Open the context menu again.
  ContextMenuWaiter menu_observer_after_network_cutoff;
  content::test::SimulateClickInFencedFrameTree(
      nested_iframe_rfh, blink::WebMouseEvent::Button::kRight, anchor_element);

  // Wait for context menu to be visible.
  menu_observer_after_network_cutoff.WaitForMenuOpenAndClose();

  // "Preview Link" should be disabled in the context menu after fenced frame
  // has untrusted network access revoked.
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedCommandIds(),
              Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW));
  EXPECT_THAT(menu_observer_after_network_cutoff.GetCapturedEnabledCommandIds(),
              Not(Contains(IDC_CONTENT_CONTEXT_OPENLINKPREVIEW)));
}

class InterestGroupContentBrowserClient : public ChromeContentBrowserClient {
 public:
  InterestGroupContentBrowserClient() = default;
  InterestGroupContentBrowserClient(const InterestGroupContentBrowserClient&) =
      delete;
  InterestGroupContentBrowserClient& operator=(
      const InterestGroupContentBrowserClient&) = delete;

  // ChromeContentBrowserClient overrides:
  // This is needed so that the interest group related APIs can run without
  // failing with the result AuctionResult::kSellerRejected.
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 content::RenderFrameHost* render_frame_host,
                                 content::InterestGroupApiOperation operation,
                                 const url::Origin& top_frame_origin,
                                 const url::Origin& api_origin) override {
    return true;
  }
};

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class ContextMenuFencedFrameTestNoTestingConfig
    : public ContextMenuFencedFrameTest {
 public:
  ContextMenuFencedFrameTestNoTestingConfig() = default;
  ~ContextMenuFencedFrameTestNoTestingConfig() override {
    content::SetBrowserClientForTesting(original_client_);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContextMenuFencedFrameTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }

  // Defines the skeleton of set up method.
  void SetUpOnMainThread() override {
    ContextMenuFencedFrameTest::SetUpOnMainThread();

    content_browser_client_ =
        std::make_unique<InterestGroupContentBrowserClient>();
    original_client_ =
        content::SetBrowserClientForTesting(content_browser_client_.get());
  }

 private:
  std::unique_ptr<InterestGroupContentBrowserClient> content_browser_client_;
  raw_ptr<content::ContentBrowserClient> original_client_;
};

// Test that automatic beacons are sent after clicking "Open Link in New Tab"
// from a contextual menu inside of a fenced frame.
IN_PROC_BROWSER_TEST_F(ContextMenuFencedFrameTestNoTestingConfig,
                       AutomaticBeaconSentAfterContextMenuNavigation) {
  privacy_sandbox::ScopedPrivacySandboxAttestations scoped_attestations(
      privacy_sandbox::PrivacySandboxAttestations::CreateForTesting());
  // Mark all Privacy Sandbox APIs as attested since the test case is testing
  // behaviors not related to attestations.
  privacy_sandbox::PrivacySandboxAttestations::GetInstance()
      ->SetAllPrivacySandboxAttestedForTesting(true);

  constexpr char kReportingURL[] = "/_report_event_server.html";
  constexpr char kBeaconMessage[] = "this is the message";

  // In order to check events reported over the network, we register an HTTP
  // response interceptor for each successful reportEvent request we expect.
  net::test_server::ControllableHttpResponse response(
      &embedded_https_test_server(), kReportingURL);

  ASSERT_TRUE(embedded_https_test_server().Start());

  auto initial_url =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  // Load a fenced frame.
  GURL fenced_frame_url(embedded_https_test_server().GetURL(
      "a.test", "/fenced_frames/title1.html"));
  GURL new_tab_url(
      embedded_https_test_server().GetURL("a.test", "/title2.html"));
  EXPECT_TRUE(ExecJs(primary_main_frame_host(),
                     "var fenced_frame = document.createElement('fencedframe');"
                     "fenced_frame.id = 'fenced_frame';"
                     "document.body.appendChild(fenced_frame);"));
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
          primary_main_frame_host());
  content::TestFrameNavigationObserver observer(fenced_frame_rfh);
  fenced_frame_test_helper().NavigateFencedFrameUsingFledge(
      primary_main_frame_host(), fenced_frame_url, "fenced_frame");
  observer.Wait();

  // Embedder-initiated fenced frame navigation uses a new browsing instance.
  // Fenced frame RenderFrameHost is a new one after navigation, so we need
  // to retrieve it.
  fenced_frame_rfh = fenced_frame_test_helper().GetMostRecentlyAddedFencedFrame(
      primary_main_frame_host());

  // Set the automatic beacon
  EXPECT_TRUE(ExecJs(
      fenced_frame_rfh,
      content::JsReplace(R"(
      window.fence.setReportEventDataForAutomaticBeacons({
        eventType: $1,
        eventData: $2,
        destination: ['seller', 'buyer']
      });
    )",
                         "reserved.top_navigation_commit", kBeaconMessage)));

  // This simulates the conditions when right clicking on a link.
  content::ContextMenuParams params;
  params.is_editable = false;
  params.media_type = blink::mojom::ContextMenuDataMediaType::kNone;
  params.page_url = fenced_frame_url;
  params.link_url = new_tab_url;

  ui_test_utils::TabAddedWaiter tab_add(browser());

  // Open the contextual menu and click "Open Link in New Tab".
  TestRenderViewContextMenu menu(*fenced_frame_rfh, params);
  menu.Init();
  menu.ExecuteCommand(IDC_CONTENT_CONTEXT_OPENLINKNEWTAB, 0);

  // Verify the automatic beacon was sent and has the correct data.
  response.WaitForRequest();
  EXPECT_EQ(response.http_request()->content, kBeaconMessage);
}

}  // namespace
