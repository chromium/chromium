// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fake_frame_widget.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class ActiveRenderWidgetHostBrowserTest : public InProcessBrowserTest {
 public:
  ActiveRenderWidgetHostBrowserTest() = default;

  ActiveRenderWidgetHostBrowserTest(const ActiveRenderWidgetHostBrowserTest&) =
      delete;
  ActiveRenderWidgetHostBrowserTest& operator=(
      const ActiveRenderWidgetHostBrowserTest&) = delete;

  ~ActiveRenderWidgetHostBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }
};

IN_PROC_BROWSER_TEST_F(ActiveRenderWidgetHostBrowserTest,
                       DocumentIsActiveAndFocused) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));

  //  Site A ------------ proxies for B C D
  //    |--Site B ------- proxies for A C D
  //    |    +--Site C -- proxies for A B D
  //    +--Site D ------- proxies for A B C
  // Where A = http://a.com/
  //       B = http://b.com/
  //       C = http://c.com/
  //       D = http://d.com/
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* main_frame_a = web_contents->GetPrimaryMainFrame();
  content::RenderFrameHost* child_frame_b = ChildFrameAt(main_frame_a, 0);
  ASSERT_NE(nullptr, child_frame_b);
  content::RenderFrameHost* child_frame_d = ChildFrameAt(main_frame_a, 1);
  ASSERT_NE(nullptr, child_frame_d);
  content::RenderFrameHost* child_frame_c = ChildFrameAt(child_frame_b, 0);
  ASSERT_NE(nullptr, child_frame_c);

  EXPECT_NE(main_frame_a->GetSiteInstance(), child_frame_b->GetSiteInstance());
  EXPECT_NE(main_frame_a->GetSiteInstance(), child_frame_d->GetSiteInstance());
  EXPECT_NE(child_frame_b->GetSiteInstance(), child_frame_c->GetSiteInstance());

  // Helper function to check document.hasFocus() for a given frame.
  // hasFocus internally calls FocusController::IsDocumentFocused which
  // return true only iff document is  active and focused.
  auto document_is_active_and_focused =
      [](content::RenderFrameHost* rfh) -> bool {
    return EvalJs(rfh, "document.hasFocus()").ExtractBool();
  };

  // Helper function to check a property of document.activeElement in the
  // specified frame.
  auto verify_active_element_property = [](content::RenderFrameHost* rfh,
                                           const std::string& property,
                                           const std::string& expected_value) {
    std::string script = base::StringPrintf(
        "document.activeElement.%s.toLowerCase();", property.c_str());
    EXPECT_EQ(expected_value, EvalJs(rfh, script));
  };

  // The main_frame_a should have a focus to start with.
  EXPECT_EQ(main_frame_a, web_contents->GetFocusedFrame());
  EXPECT_TRUE(document_is_active_and_focused(main_frame_a));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_b));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_c));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_d));
  verify_active_element_property(main_frame_a, "tagName", "body");

  // After focusing child_frame_b, document.hasFocus() should return
  // true for child_frame_b and all its ancestor frames.
  EXPECT_TRUE(ExecJs(child_frame_b, "window.focus();"));
  EXPECT_EQ(child_frame_b, web_contents->GetFocusedFrame());
  EXPECT_TRUE(document_is_active_and_focused(main_frame_a));
  EXPECT_TRUE(document_is_active_and_focused(child_frame_b));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_c));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_d));
  verify_active_element_property(main_frame_a, "tagName", "iframe");
  verify_active_element_property(main_frame_a, "src",
                                 child_frame_b->GetLastCommittedURL().spec());

  // After focusing child_frame_c, document.hasFocus() should return
  // true for child_frame_c and all its ancestor frames.
  EXPECT_TRUE(ExecJs(child_frame_c, "window.focus();"));
  EXPECT_EQ(child_frame_c, web_contents->GetFocusedFrame());
  EXPECT_TRUE(document_is_active_and_focused(main_frame_a));
  EXPECT_TRUE(document_is_active_and_focused(child_frame_b));
  EXPECT_TRUE(document_is_active_and_focused(child_frame_c));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_d));
  verify_active_element_property(main_frame_a, "tagName", "iframe");
  // Check document.activeElement in main_frame_a.  It should still
  // point to <iframe> for the b.com frame, since Blink computes the
  // focused iframe element by walking the parent chain of the focused
  // frame until it hits the current frame.  This logic should still
  // work with remote frames.
  verify_active_element_property(main_frame_a, "src",
                                 child_frame_b->GetLastCommittedURL().spec());

  // After focusing child_frame_d, document.hasFocus() should return
  // true for child_frame_d and all its ancestor frames.
  EXPECT_TRUE(ExecJs(child_frame_d, "window.focus();"));
  EXPECT_EQ(child_frame_d, web_contents->GetFocusedFrame());
  EXPECT_TRUE(document_is_active_and_focused(main_frame_a));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_b));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_c));
  EXPECT_TRUE(document_is_active_and_focused(child_frame_d));
  verify_active_element_property(main_frame_a, "tagName", "iframe");
  verify_active_element_property(main_frame_a, "src",
                                 child_frame_d->GetLastCommittedURL().spec());

  // After focusing main_frame_a, document.hasFocus() should return
  // true for main_frame_a and since it's a root of tree, all its
  // descendants should return false. On the renderer side, both the
  // 'active' and 'focus' states for blink::FocusController will be
  // true.
  EXPECT_TRUE(ExecJs(main_frame_a, "window.focus();"));
  EXPECT_EQ(main_frame_a, web_contents->GetFocusedFrame());
  EXPECT_TRUE(document_is_active_and_focused(main_frame_a));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_b));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_c));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_d));
  verify_active_element_property(main_frame_a, "tagName", "body");

  // Focus the URL bar.
  OmniboxView* omnibox =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  // Give the omnibox focus.
  omnibox->SetFocus(/*is_user_initiated=*/true);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(main_frame_a, web_contents->GetFocusedFrame());

  // `omnibox->SetFocus()` should call blur event on main_frame_a and
  // deactivate the active render widget, but on Mac calling
  // `omnibox->SetFocus()` function doesn't invoke
  // RWHI::SetActive(false). As a result, `blink::FocusController`'s
  // 'active' state maintains the previous value of false.
  //
  // This table sums up `blink::FocusController`'s 'active' and 'focus'
  // states on different platforms after focusing the omnibox:
  //
  // |        | Linux |  Mac  | Windows |
  // | active | false | true  | false   |
  // | focus  | false | false | false   |
  //
  // Since `document.hasFocus()` only returns true iff the document is
  // both active and focus, the test still expects
  // `document.hasFocus()` to be false on all platforms.
  //
  // Note that there is no separate API to test active state of the
  // document. Instead, Mac's active behavior is separately tested in
  // `ActiveRenderWidgetHostBrowserTest.FocusOmniBox`.
  EXPECT_FALSE(document_is_active_and_focused(main_frame_a));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_b));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_c));
  EXPECT_FALSE(document_is_active_and_focused(child_frame_d));
  // body tag is active by default.
  verify_active_element_property(main_frame_a, "tagName", "body");
  verify_active_element_property(child_frame_b, "tagName", "body");
  verify_active_element_property(child_frame_c, "tagName", "body");
  verify_active_element_property(child_frame_d, "tagName", "body");
}

// This test verifies that on Mac, moving the focus from webcontents to Omnibox
// doesn't change the 'active' state and old value of the active state is
// retained.
//
// FakeFrameWidget has Optional<bool> 'active' state which is
// uninitialised at the beginning. omnibox->SetFocus() invokes
// RWHI::SetActive(false) for webcontents and there is a IPC call to
// renderer which changes 'active' state to false.
//
// On Mac, calling omnibox->SetFocus function doesn't invoke
// RWHI::SetActive(false). Hence there is no IPC call to renderer and
// 'active' state maintains old value.
IN_PROC_BROWSER_TEST_F(ActiveRenderWidgetHostBrowserTest, FocusOmniBox) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget>
      blink_frame_widget_receiver =
          content::BindFakeFrameWidgetInterfaces(main_frame);
  content::FakeFrameWidget fake_frame_widget(
      std::move(blink_frame_widget_receiver));

  // Main frame is already focused at this point and now focus URL bar.
  OmniboxView* omnibox =
      browser()->window()->GetLocationBar()->GetOmniboxView();
  // Give the omnibox focus.
  omnibox->SetFocus(/*is_user_initiated=*/true);

  base::RunLoop().RunUntilIdle();
#if BUILDFLAG(IS_MAC)
  // On MacOS, calling omnibox->SetFocus function doesn't invoke
  // RWHI::SetActive. Hence there is no IPC call to renderer and
  // FakeFrameWidget's 'active' state remains uninitialised.
  EXPECT_EQ(fake_frame_widget.GetActive(), std::nullopt);
#else
  EXPECT_EQ(fake_frame_widget.GetActive(), false);
#endif
}
