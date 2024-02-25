// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/pdf/pdf_extension_test_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/focused_node_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/focus_changed_observer.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/test/geometry_util.h"
#include "ui/gfx/selection_bound.h"
#include "ui/touch_selection/touch_selection_controller.h"
#include "ui/views/touchui/touch_selection_menu_views.h"
#include "ui/views/widget/any_widget_observer.h"
#include "ui/views/widget/widget.h"
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)

namespace {

using ::pdf_extension_test_util::ConvertPageCoordToScreenCoord;
using ::pdf_extension_test_util::GetOnlyMimeHandlerView;
using ::pdf_extension_test_util::SetInputFocusOnPlugin;

class PDFExtensionInteractiveUITest : public base::test::WithFeatureOverride,
                                      public PDFExtensionTestBase {
 public:
  PDFExtensionInteractiveUITest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  content::FocusedNodeDetails TabAndWait(
      extensions::MimeHandlerViewGuest* guest,
      bool forward) {
    content::FocusChangedObserver focus_observer(guest->web_contents());
    if (!ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_TAB,
                                         /*control=*/false,
                                         /*shift=*/!forward,
                                         /*alt=*/false,
                                         /*command=*/false)) {
      ADD_FAILURE() << "Failed to send key press";
      return {};
    }
    return focus_observer.Wait();
  }

  bool UseOopif() const override { return GetParam(); }
};

class TabChangedWaiter : public TabStripModelObserver {
 public:
  explicit TabChangedWaiter(Browser* browser) {
    browser->tab_strip_model()->AddObserver(this);
  }
  TabChangedWaiter(const TabChangedWaiter&) = delete;
  TabChangedWaiter& operator=(const TabChangedWaiter&) = delete;
  ~TabChangedWaiter() override = default;

  void Wait() { run_loop_.Run(); }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (change.type() == TabStripModelChange::kSelectionOnly)
      run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
};

}  // namespace

// For crbug.com/1038918
IN_PROC_BROWSER_TEST_P(PDFExtensionInteractiveUITest,
                       CtrlPageUpDownSwitchesTabs) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  extensions::MimeHandlerViewGuest* guest = LoadPdfInNewTabGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf"));

  auto* tab_strip_model = browser()->tab_strip_model();
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(1, tab_strip_model->active_index());

  SetInputFocusOnPlugin(guest);

  {
    TabChangedWaiter tab_changed_waiter(browser());
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_NEXT,
                                                /*control=*/true,
                                                /*shift=*/false,
                                                /*alt=*/false,
                                                /*command=*/false));
    tab_changed_waiter.Wait();
  }
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(0, tab_strip_model->active_index());

  {
    TabChangedWaiter tab_changed_waiter(browser());
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_PRIOR,
                                                /*control=*/true,
                                                /*shift=*/false,
                                                /*alt=*/false,
                                                /*command=*/false));
    tab_changed_waiter.Wait();
  }
  ASSERT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(1, tab_strip_model->active_index());
}

IN_PROC_BROWSER_TEST_P(PDFExtensionInteractiveUITest, FocusForwardTraversal) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  extensions::MimeHandlerViewGuest* guest = LoadPdfInNewTabGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf#toolbar=0"));

  // Tab in.
  content::FocusedNodeDetails details = TabAndWait(guest, /*forward=*/true);
  EXPECT_EQ(blink::mojom::FocusType::kForward, details.focus_type);

  // Tab out.
  details = TabAndWait(guest, /*forward=*/true);
  EXPECT_EQ(blink::mojom::FocusType::kNone, details.focus_type);
}

IN_PROC_BROWSER_TEST_P(PDFExtensionInteractiveUITest, FocusReverseTraversal) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  extensions::MimeHandlerViewGuest* guest = LoadPdfInNewTabGetMimeHandlerView(
      embedded_test_server()->GetURL("/pdf/test.pdf#toolbar=0"));

  // Tab in.
  content::FocusedNodeDetails details = TabAndWait(guest, /*forward=*/false);
  EXPECT_EQ(blink::mojom::FocusType::kBackward, details.focus_type);

  // Tab out.
  details = TabAndWait(guest, /*forward=*/false);
  EXPECT_EQ(blink::mojom::FocusType::kNone, details.focus_type);
}

#if defined(TOOLKIT_VIEWS) && defined(USE_AURA)
namespace {

views::Widget* TouchSelectText(content::WebContents* contents,
                               const gfx::Point& screen_pos) {
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       "TouchSelectionMenuViews");
  content::SimulateTouchEventAt(contents, ui::ET_TOUCH_PRESSED, screen_pos);

  EXPECT_EQ(true, content::EvalJs(
                      contents,
                      "new Promise(resolve => {"
                      "  window.addEventListener('message', function(event) {"
                      "    if (event.data.type == 'touchSelectionOccurred')"
                      "      resolve(true);"
                      "  });"
                      "});"));

  content::SimulateTouchEventAt(contents, ui::ET_TOUCH_RELEASED, screen_pos);
  return waiter.WaitIfNeededAndGet();
}

}  // namespace

// On text selection, a touch selection menu should pop up. On clicking ellipsis
// icon on the menu, the context menu should open up.
IN_PROC_BROWSER_TEST_P(PDFExtensionInteractiveUITest,
                       ContextMenuOpensFromTouchSelectionMenu) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  const GURL url = embedded_test_server()->GetURL("/pdf/text_large.pdf");
  extensions::MimeHandlerViewGuest* guest =
      LoadPdfInNewTabGetMimeHandlerView(url);
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  const gfx::Point point_in_root_coords =
      guest_mainframe->GetView()->TransformPointToRootCoordSpace(
          ConvertPageCoordToScreenCoord(guest_mainframe, {12, 12}));
  views::Widget* widget =
      TouchSelectText(GetActiveWebContents(), point_in_root_coords);
  ASSERT_TRUE(widget);
  views::View* menu = widget->GetContentsView();
  ASSERT_TRUE(menu);
  views::View* ellipsis_button = menu->GetViewByID(
      views::TouchSelectionMenuViews::ButtonViewId::kEllipsisButton);
  ASSERT_TRUE(ellipsis_button);
  ContextMenuWaiter context_menu_observer;
  ui::GestureEvent tap(0, 0, 0, ui::EventTimeForNow(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  ellipsis_button->OnGestureEvent(&tap);
  context_menu_observer.WaitForMenuOpenAndClose();

  // Verify that the expected context menu items are present.
  //
  // Note that the assertion below doesn't use exact matching via
  // testing::ElementsAre, because some platforms may include unexpected extra
  // elements (e.g. an extra separator and IDC=100 has been observed on some Mac
  // bots).
  EXPECT_THAT(
      context_menu_observer.GetCapturedCommandIds(),
      testing::IsSupersetOf(
          {IDC_CONTENT_CONTEXT_COPY, IDC_CONTENT_CONTEXT_SEARCHWEBFOR,
           IDC_PRINT, IDC_CONTENT_CONTEXT_ROTATECW,
           IDC_CONTENT_CONTEXT_ROTATECCW, IDC_CONTENT_CONTEXT_INSPECTELEMENT}));
}

// TODO(crbug.com/1335822): Deflake this test.
#if BUILDFLAG(IS_WIN)
#define MAYBE_TouchSelectionBounds DISABLED_TouchSelectionBounds
#else
#define MAYBE_TouchSelectionBounds TouchSelectionBounds
#endif  // BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_P(PDFExtensionInteractiveUITest,
                       MAYBE_TouchSelectionBounds) {
  // TODO(crbug.com/1445746): Remove this once the test passes for OOPIF PDF.
  if (UseOopif()) {
    GTEST_SKIP();
  }

  // Use test.pdf here because it has embedded font metrics. With a fixed zoom,
  // coordinates should be consistent across platforms.
  const GURL url = embedded_test_server()->GetURL("/pdf/test.pdf#zoom=100");
  extensions::MimeHandlerViewGuest* guest =
      LoadPdfInNewTabGetMimeHandlerView(url);
  ASSERT_TRUE(guest);

  content::RenderFrameHost* guest_mainframe = guest->GetGuestMainFrame();
  ASSERT_TRUE(guest_mainframe);
  content::WaitForHitTestData(guest_mainframe);

  views::Widget* widget = TouchSelectText(GetActiveWebContents(), {473, 166});
  ASSERT_TRUE(widget);

  auto* touch_selection_controller =
      guest_mainframe->GetView()
          ->GetTouchSelectionControllerClientManager()
          ->GetTouchSelectionController();

  gfx::SelectionBound start_bound = touch_selection_controller->start();
  EXPECT_EQ(gfx::SelectionBound::LEFT, start_bound.type());
  EXPECT_POINTF_NEAR(gfx::PointF(454.0f, 161.0f), start_bound.edge_start(),
                     1.0f);
  EXPECT_POINTF_NEAR(gfx::PointF(454.0f, 171.0f), start_bound.edge_end(), 1.0f);

  gfx::SelectionBound end_bound = touch_selection_controller->end();
  EXPECT_EQ(gfx::SelectionBound::RIGHT, end_bound.type());
  EXPECT_POINTF_NEAR(gfx::PointF(492.0f, 161.0f), end_bound.edge_start(), 1.0f);
  EXPECT_POINTF_NEAR(gfx::PointF(492.0f, 171.0f), end_bound.edge_end(), 1.0f);
}
#endif  // defined(TOOLKIT_VIEWS) && defined(USE_AURA)

// TODO(crbug.com/1445746): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(PDFExtensionInteractiveUITest);
