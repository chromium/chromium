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
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/metrics/subprocess_metrics_provider.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pdf_util.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
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
#include "extensions/common/guest_view/mime_handler_view_uma_types.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_response_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "url/url_constants.h"
#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using extensions::ExtensionsAPIClient;
using extensions::MimeHandlerViewGuest;
using extensions::TestMimeHandlerViewGuest;
using guest_view::GuestViewManager;
using guest_view::TestGuestViewManager;
using guest_view::TestGuestViewManagerFactory;
using UMAType = extensions::MimeHandlerViewUMATypes::Type;

namespace {
constexpr char kTestExtensionId[] = "oickdpebdnfbgkcaoklfcdhjniefkcji";
}

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
class ChromeMimeHandlerViewTestBase : public extensions::ExtensionApiTest {
 public:
  ChromeMimeHandlerViewTestBase() {
    GuestViewManager::set_factory_for_testing(&factory_);
  }

  ~ChromeMimeHandlerViewTestBase() override {}

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
    CHECK_EQ(kTestExtensionId, extension->id());

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
    auto transferrable_loader = content::mojom::TransferrableURLLoader::New();
    transferrable_loader->url = url;
    transferrable_loader->head = network::mojom::URLResponseHead::New();
    transferrable_loader->head->mime_type = "application/pdf";
    transferrable_loader->head->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/2 200 OK");
    return std::make_unique<extensions::StreamContainer>(
        0 /* tab_id */, false /* embedded */,
        GURL(std::string(extensions::kExtensionScheme) +
             kTestExtensionId) /* handler_url */,
        kTestExtensionId, std::move(transferrable_loader), url);
  }

 private:
  TestGuestViewManagerFactory factory_;
  content::WebContents* guest_web_contents_;
  content::WebContents* embedder_web_contents_;

  ChromeMimeHandlerViewTestBase(const ChromeMimeHandlerViewTestBase&) = delete;
  ChromeMimeHandlerViewTestBase& operator=(
      const ChromeMimeHandlerViewTestBase&) = delete;
};

// The parametric version of the test class which runs the test both on
// BrowserPlugin-based and cross-process-frame-based MimeHandlerView
// implementation. All current browser tests should eventually be moved to this
// and then eventually drop the BrowserPlugin dependency once
// https://crbug.com/659750 is fixed.
class ChromeMimeHandlerViewCrossProcessTest
    : public ChromeMimeHandlerViewTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  ChromeMimeHandlerViewCrossProcessTest() : ChromeMimeHandlerViewTestBase() {}
  ~ChromeMimeHandlerViewCrossProcessTest() override {}

  void SetUpCommandLine(base::CommandLine* cl) override {
    ChromeMimeHandlerViewTestBase::SetUpCommandLine(cl);
    is_cross_process_mode_ = GetParam();
    if (is_cross_process_mode_) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kMimeHandlerViewInCrossProcessFrame);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kMimeHandlerViewInCrossProcessFrame);
    }
  }

  bool is_cross_process_mode() const { return is_cross_process_mode_; }

 private:
  bool is_cross_process_mode_ = false;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ChromeMimeHandlerViewCrossProcessTest);
};

// A class of tests which were originally designed as WebViewGuest tests which
// were testing some aspects of BrowserPlugin. Since all GuestViews except for
// MimeHandlerViewGuest have now moved on to using cross-process frames these
// tests were modified to using MimeHandlerViewGuest instead. They also could
// not be moved to extensions/browser/guest_view/mime_handler_view due to chrome
// layer dependencies.
class ChromeMimeHandlerViewBrowserPluginTest
    : public ChromeMimeHandlerViewTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* cl) override {
    ChromeMimeHandlerViewTestBase::SetUpCommandLine(cl);
    scoped_feature_list_.InitAndDisableFeature(
        features::kMimeHandlerViewInCrossProcessFrame);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Helper class to monitor focus on a WebContents with BrowserPlugin (guest).
class FocusChangeWaiter {
 public:
  explicit FocusChangeWaiter(content::WebContents* web_contents,
                             bool expected_focus)
      : web_contents_(web_contents), expected_focus_(expected_focus) {}
  ~FocusChangeWaiter() {}

  void WaitForFocusChange() {
    while (expected_focus_ !=
           IsWebContentsBrowserPluginFocused(web_contents_)) {
      base::RunLoop().RunUntilIdle();
    }
  }

 private:
  content::WebContents* web_contents_;
  bool expected_focus_;
};

// This test creates two guest views in a tab: a normal attached
// MimeHandlerViewGuest, and then another MHVG which is unattached. Right after
// the second GuestView's WebContents is created a find request is send to the
// tab's WebContents. The test then verifies that the set of outstanding
// RenderFrameHosts with find request in flight includes all frames but the one
// from the unattached guest. For more context see https://crbug.com/897465.
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       NoFindInPageForUnattachedGuest) {
  InitializeTestPage(embedded_test_server()->GetURL("/testBasic.csv"));
  auto* main_frame = embedder_web_contents()->GetMainFrame();
  auto* attached_guest_main_frame = guest_web_contents()->GetMainFrame();
  std::string view_id;
  auto stream_container = CreateFakeStreamContainer(GURL("foo.com"), &view_id);
  extensions::MimeHandlerStreamManager::Get(
      embedder_web_contents()->GetBrowserContext())
      ->AddStream(view_id, std::move(stream_container),
                  main_frame->GetFrameTreeNodeId(),
                  main_frame->GetProcess()->GetID(),
                  main_frame->GetRoutingID());
  base::DictionaryValue create_params;
  create_params.SetString(mime_handler_view::kViewId, view_id);
  // The actual test logic is inside the callback.
  GuestViewManager::WebContentsCreatedCallback callback = base::BindOnce(
      [](content::WebContents* embedder_contents,
         content::RenderFrameHost* attached_guest_rfh,
         content::WebContents* guest_contents) {
        auto* guest_main_frame = guest_contents->GetMainFrame();
        auto* find_helper = FindTabHelper::FromWebContents(embedder_contents);
        find_helper->StartFinding(base::ASCIIToUTF16("doesn't matter"), true,
                                  true, false);
        auto pending = content::GetRenderFrameHostsWithPendingFindResults(
            embedder_contents);
        // Request for main frame of the tab.
        EXPECT_EQ(1U, pending.count(embedder_contents->GetMainFrame()));
        // Request for main frame of the attached guest.
        EXPECT_EQ(1U, pending.count(attached_guest_rfh));
        // No request for the unattached guest.
        EXPECT_EQ(0U, pending.count(guest_main_frame));
        // Sanity-check: try the set returned for guest.
        pending =
            content::GetRenderFrameHostsWithPendingFindResults(guest_contents);
        EXPECT_TRUE(pending.empty());
      },
      embedder_web_contents(), attached_guest_main_frame);
  GetGuestViewManager()->CreateGuest(MimeHandlerViewGuest::Type,
                                     embedder_web_contents(), create_params,
                                     std::move(callback));
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       UnderChildFrame) {
  // Create this frame tree structure.
  // main_frame_node
  //   |
  // middle_node -> is child of |main_frame_node|
  //   |
  // mime_node  -> is inner web contents of |middle_node|
  InitializeTestPage(
      embedded_test_server()->GetURL("/find_in_page_one_frame.html"));
  int ordinal;
  EXPECT_EQ(2, ui_test_utils::FindInPage(embedder_web_contents(),
                                         base::ASCIIToUTF16("two"), true, true,
                                         &ordinal, nullptr));
  EXPECT_EQ(1, ordinal);
  // Go to next result.
  EXPECT_EQ(2, ui_test_utils::FindInPage(embedder_web_contents(),
                                         base::ASCIIToUTF16("two"), true, true,
                                         &ordinal, nullptr));
  EXPECT_EQ(2, ordinal);
  // Go to next result, should wrap back to #1.
  EXPECT_EQ(2, ui_test_utils::FindInPage(embedder_web_contents(),
                                         base::ASCIIToUTF16("two"), true, true,
                                         &ordinal, nullptr));
  EXPECT_EQ(1, ordinal);
}

// Flaky under MSan: https://crbug.com/837757
#if defined(MEMORY_SANITIZER)
#define MAYBE_BP_AutoResizeMessages DISABLED_AutoResizeMessages
#else
#define MAYBE_BP_AutoResizeMessages AutoResizeMessages
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       MAYBE_BP_AutoResizeMessages) {
  InitializeTestPage(embedded_test_server()->GetURL("/testBasic.csv"));

  // Helper function as this test requires inspecting a number of content::
  // internal objects.
  EXPECT_TRUE(content::TestChildOrGuestAutoresize(
      true,
      embedder_web_contents()
          ->GetRenderWidgetHostView()
          ->GetRenderWidgetHost()
          ->GetProcess(),
      guest_web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost()));
}

#if defined(USE_AURA)
// Flaky on Linux. See: https://crbug.com/870604.
#if defined(OS_LINUX)
#define MAYBE_TouchFocusesEmbedder DISABLED_TouchFocusesEmbedder
#else
#define MAYBE_TouchFocusesEmbedder TouchFocusesEmbedder
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       MAYBE_TouchFocusesEmbedder) {
  InitializeTestPage(embedded_test_server()->GetURL("/testBasic.csv"));

  content::RenderViewHost* embedder_rvh =
      embedder_web_contents()->GetRenderViewHost();
  content::RenderFrameSubmissionObserver frame_observer(
      embedder_web_contents());

  bool embedder_has_touch_handler =
      content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh);
  ASSERT_FALSE(embedder_has_touch_handler);

  ASSERT_TRUE(ExecuteScript(
      guest_web_contents(),
      "document.addEventListener('touchstart', dummyTouchStartHandler);"));
  // Wait until embedder has touch handlers.
  while (!content::RenderViewHostTester::HasTouchEventHandler(embedder_rvh)) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  auto* top_level_window =
      embedder_web_contents()->GetNativeView()->GetToplevelWindow();
  ASSERT_TRUE(top_level_window);
  auto* widget = views::Widget::GetWidgetForNativeWindow(top_level_window);
  ASSERT_TRUE(widget);
  ASSERT_TRUE(widget->GetRootView());

  // Find WebView corresponding to embedder_web_contents().
  const std::string kWebViewClassName = views::WebView::kViewClassName;
  views::View* aura_webview = nullptr;
  for (base::circular_deque<views::View*> deque = {widget->GetRootView()};
       !deque.empty(); deque.pop_front()) {
    views::View* current = deque.front();
    if (current->GetClassName() == kWebViewClassName &&
        static_cast<views::WebView*>(current)->GetWebContents() ==
            embedder_web_contents()) {
      aura_webview = current;
      break;
    }
    const auto& children = current->children();
    deque.insert(deque.end(), children.cbegin(), children.cend());
  }
  ASSERT_TRUE(aura_webview);
  gfx::Rect bounds(aura_webview->bounds());
  EXPECT_TRUE(aura_webview->IsFocusable());

  views::View* other_focusable_view = new views::View();
  other_focusable_view->SetBounds(bounds.x() + bounds.width(), bounds.y(), 100,
                                  100);
  other_focusable_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);
  // Focusable views require an accessible name to pass accessibility checks.
  other_focusable_view->GetViewAccessibility().OverrideName("Any name");
  aura_webview->parent()->AddChildView(other_focusable_view);
  other_focusable_view->SetPosition(gfx::Point(bounds.x() + bounds.width(), 0));

  // Sync changes to compositor.
  while (!RequestFrame(embedder_web_contents())) {
    // RequestFrame failed because we were waiting on an ack ... wait a short
    // time and retry.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(10));
    run_loop.Run();
  }
  frame_observer.WaitForAnyFrameSubmission();

  aura_webview->RequestFocus();
  // Verify that other_focusable_view can steal focus from aura_webview.
  EXPECT_TRUE(aura_webview->HasFocus());
  other_focusable_view->RequestFocus();
  EXPECT_TRUE(other_focusable_view->HasFocus());
  EXPECT_FALSE(aura_webview->HasFocus());

  // Compute location of guest within embedder so we can more accurately
  // target our touch event.
  gfx::Rect guest_rect = guest_web_contents()->GetContainerBounds();
  gfx::Point embedder_origin =
      embedder_web_contents()->GetContainerBounds().origin();
  guest_rect.Offset(-embedder_origin.x(), -embedder_origin.y());

  // Generate and send synthetic touch event.
  content::InputEventAckWaiter waiter(
      guest_web_contents()->GetRenderWidgetHostView()->GetRenderWidgetHost(),
      blink::WebInputEvent::kTouchStart);
  content::SimulateTouchPressAt(embedder_web_contents(),
                                guest_rect.CenterPoint());
  waiter.Wait();
  EXPECT_TRUE(aura_webview->HasFocus());
}

IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginTest,
                       TouchFocusesBrowserPluginInEmbedder) {
  InitializeTestPage(embedded_test_server()->GetURL("/test_embedded.html"));

  auto embedder_rect = embedder_web_contents()->GetContainerBounds();
  auto guest_rect = guest_web_contents()->GetContainerBounds();

  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());
  embedder_rect.set_x(0);
  embedder_rect.set_y(0);

  // Don't send events that need to be routed until we know the child's surface
  // is ready for hit testing.
  content::WaitForHitTestData(guest_web_contents());

  // 1) BrowserPlugin should not be focused at start.
  ASSERT_FALSE(IsWebContentsBrowserPluginFocused(guest_web_contents()));

  // 2) Send touch event to guest, now BrowserPlugin should get focus.
  {
    gfx::Point point = guest_rect.CenterPoint();
    FocusChangeWaiter focus_waiter(guest_web_contents(), true);
    SendRoutedTouchTapSequence(embedder_web_contents(), point);
    SendRoutedGestureTapSequence(embedder_web_contents(), point);
    focus_waiter.WaitForFocusChange();
    ASSERT_TRUE(IsWebContentsBrowserPluginFocused(guest_web_contents()));
  }

  // 3) Send touch start to embedder, now BrowserPlugin should lose focus.
  {
    // Choose a point outside of guest (but inside the embedder).
    gfx::Point point = guest_rect.bottom_right();
    point += gfx::Vector2d(10, 10);
    EXPECT_TRUE(embedder_rect.Contains(point));
    FocusChangeWaiter focus_waiter(guest_web_contents(), false);
    SendRoutedTouchTapSequence(embedder_web_contents(), point);
    SendRoutedGestureTapSequence(embedder_web_contents(), point);
    focus_waiter.WaitForFocusChange();
    ASSERT_FALSE(IsWebContentsBrowserPluginFocused(guest_web_contents()));
  }
}
#endif  // USE_AURA

class ChromeMimeHandlerViewBrowserPluginScrollTest
    : public ChromeMimeHandlerViewBrowserPluginTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ChromeMimeHandlerViewBrowserPluginTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(
        switches::kTouchEventFeatureDetection,
        switches::kTouchEventFeatureDetectionEnabled);
  }
};

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_MACOSX)
#define MAYBE_ScrollGuestContent DISABLED_ScrollGuestContent
#else
#define MAYBE_ScrollGuestContent ScrollGuestContent
#endif
IN_PROC_BROWSER_TEST_F(ChromeMimeHandlerViewBrowserPluginScrollTest,
                       MAYBE_ScrollGuestContent) {
  InitializeTestPage(embedded_test_server()->GetURL("/test_embedded.html"));

  ASSERT_TRUE(ExecuteScript(guest_web_contents(), "ensurePageIsScrollable();"));

  content::RenderFrameSubmissionObserver embedder_frame_observer(
      embedder_web_contents());
  content::RenderFrameSubmissionObserver guest_frame_observer(
      guest_web_contents());

  gfx::Rect embedder_rect = embedder_web_contents()->GetContainerBounds();
  gfx::Rect guest_rect = guest_web_contents()->GetContainerBounds();
  guest_rect.set_x(guest_rect.x() - embedder_rect.x());
  guest_rect.set_y(guest_rect.y() - embedder_rect.y());

  gfx::Vector2dF default_offset;
  guest_frame_observer.WaitForScrollOffset(default_offset);
  embedder_frame_observer.WaitForScrollOffset(default_offset);

  gfx::Point guest_scroll_location(guest_rect.width() / 2,
                                   guest_rect.height() / 2);
  float gesture_distance = 15.f;
  {
    gfx::Vector2dF expected_offset(0.f, gesture_distance);

    content::SimulateGestureScrollSequence(
        guest_web_contents(), guest_scroll_location,
        gfx::Vector2dF(0, -gesture_distance));

    guest_frame_observer.WaitForScrollOffset(expected_offset);
  }

  embedder_frame_observer.WaitForScrollOffset(default_offset);

  // Use fling gesture to scroll back, velocity should be big enough to scroll
  // content back.
  float fling_velocity = 300.f;
  {
    content::SimulateGestureFlingSequence(guest_web_contents(),
                                          guest_scroll_location,
                                          gfx::Vector2dF(0, fling_velocity));

    guest_frame_observer.WaitForScrollOffset(default_offset);
  }

  embedder_frame_observer.WaitForScrollOffset(default_offset);
}

IN_PROC_BROWSER_TEST_P(ChromeMimeHandlerViewCrossProcessTest,
                       UMA_SameOriginResource) {
  auto url = embedded_test_server()->GetURL("a.com", "/testPostMessageUMA.csv");
  auto page_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_postmessage_uma.html?%s", url.spec().c_str()));
  InitializeTestPage(page_url);
  EXPECT_TRUE(ExecJs(embedder_web_contents(), "sendMessages();"));
  const std::vector<std::pair<extensions::MimeHandlerViewUMATypes::Type, int>>
      kTestCases = {
          {(GetParam() ? UMAType::kCreateFrameContainer
                       : UMAType::kDidCreateMimeHandlerViewContainerBase),
           1},
          {UMAType::kDidLoadExtension, 1},
          {UMAType::kAccessibleInvalid, 1},
          {UMAType::kAccessibleSelectAll, 1},
          {UMAType::kAccessibleGetSelectedText, 1},
          {UMAType::kAccessiblePrint, 2},
          {UMAType::kPostMessageToEmbeddedMimeHandlerView, 5}};
  base::HistogramTester histogram_tester;
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  for (const auto& pair : kTestCases) {
    histogram_tester.ExpectBucketCount(
        extensions::MimeHandlerViewUMATypes::kUMAName, pair.first, pair.second);
  }
}

IN_PROC_BROWSER_TEST_P(ChromeMimeHandlerViewCrossProcessTest,
                       UMA_CrossOriginResource) {
  auto url = embedded_test_server()->GetURL("b.com", "/testPostMessageUMA.csv");
  auto page_url = embedded_test_server()->GetURL(
      "a.com",
      base::StringPrintf("/test_postmessage_uma.html?%s", url.spec().c_str()));
  InitializeTestPage(page_url);
  EXPECT_TRUE(ExecJs(embedder_web_contents(), "sendMessages();"));
  const std::vector<std::pair<extensions::MimeHandlerViewUMATypes::Type, int>>
      kTestCases = {
          {(GetParam() ? UMAType::kCreateFrameContainer
                       : UMAType::kDidCreateMimeHandlerViewContainerBase),
           1},
          {UMAType::kDidLoadExtension, 1},
          {UMAType::kInaccessibleInvalid, 1},
          {UMAType::kInaccessibleSelectAll, 1},
          {UMAType::kInaccessibleGetSelectedText, 1},
          {UMAType::kInaccessiblePrint, 2},
          {UMAType::kPostMessageToEmbeddedMimeHandlerView, 5}};
  base::HistogramTester histogram_tester;
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  for (const auto& pair : kTestCases) {
    histogram_tester.ExpectBucketCount(
        extensions::MimeHandlerViewUMATypes::kUMAName, pair.first, pair.second);
  }
}

IN_PROC_BROWSER_TEST_P(ChromeMimeHandlerViewCrossProcessTest,
                       UMAPDFLoadStatsFullPage) {
  base::HistogramTester histogram_tester;
  GURL data_url("data:application/pdf,foo");
  ui_test_utils::NavigateToURL(browser(), data_url);
  auto* guest = GetGuestViewManager()->WaitForSingleGuestCreated();
  while (guest->IsLoading()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "PDF.LoadStatus", PDFLoadStatus::kLoadedFullPagePdfWithPdfium, 1);
}

IN_PROC_BROWSER_TEST_P(ChromeMimeHandlerViewCrossProcessTest,
                       UMAPDFLoadStatsEmbedded) {
  base::HistogramTester histogram_tester;
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  ASSERT_TRUE(content::ExecJs(
      browser()->tab_strip_model()->GetWebContentsAt(0),
      "document.write('<iframe></iframe>');"
      "document.querySelector('iframe').src = 'data:application/pdf, foo';"));
  auto* guest = GetGuestViewManager()->WaitForSingleGuestCreated();
  while (guest->IsLoading()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
  SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
  histogram_tester.ExpectBucketCount(
      "PDF.LoadStatus", PDFLoadStatus::kLoadedEmbeddedPdfWithPdfium, 1);
}

namespace {

// A DevToolsAgentHostClient implementation doing nothing.
class StubDevToolsAgentHostClient : public content::DevToolsAgentHostClient {
 public:
  StubDevToolsAgentHostClient() {}
  ~StubDevToolsAgentHostClient() override {}
  void AgentHostClosed(content::DevToolsAgentHost* agent_host) override {}
  void DispatchProtocolMessage(content::DevToolsAgentHost* agent_host,
                               const std::string& message) override {}
};

}  // namespace

IN_PROC_BROWSER_TEST_P(ChromeMimeHandlerViewCrossProcessTest,
                       GuestDevToolsReloadsEmbedder) {
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
  devtools_agent_host->DispatchProtocolMessage(
      &devtools_agent_host_client, R"({"id":1,"method": "Page.reload"})");
  reload_observer.Wait();
  devtools_agent_host->DetachClient(&devtools_agent_host_client);
}

// This test verifies that a display:none frame loading a MimeHandlerView type
// will end up creating a MimeHandlerview. NOTE: this is an exception to support
// printing in Google docs (see https://crbug.com/978240).
IN_PROC_BROWSER_TEST_P(ChromeMimeHandlerViewCrossProcessTest,
                       MimeHandlerViewInDisplayNoneFrameForGoogleApps) {
  GURL data_url(
      "data:text/html, <iframe src='data:application/pdf,foo' "
      "style='display:none'></iframe>,foo");
  ui_test_utils::NavigateToURL(browser(), data_url);
  ASSERT_TRUE(GetGuestViewManager()->WaitForSingleGuestCreated());
}

INSTANTIATE_TEST_SUITE_P(,
                         ChromeMimeHandlerViewCrossProcessTest,
                         testing::Bool());
