// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/autoclick_test_utils.h"
#include "chrome/browser/ash/accessibility/caret_bounds_changed_waiter.h"
#include "chrome/browser/ash/accessibility/html_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/browsertest_util.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {

class AutoclickBrowserTest : public InProcessBrowserTest {
 public:
  AutoclickBrowserTest(const AutoclickBrowserTest&) = delete;
  AutoclickBrowserTest& operator=(const AutoclickBrowserTest&) = delete;

 protected:
  AutoclickBrowserTest() = default;
  ~AutoclickBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    autoclick_test_utils_ =
        std::make_unique<AutoclickTestUtils>(browser()->profile());
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void TearDownOnMainThread() override { autoclick_test_utils_.reset(); }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  PrefService* GetPrefs() { return browser()->profile()->GetPrefs(); }

  // Loads a page with the given URL and then starts up Autoclick.
  void LoadURLAndAutoclick(const std::string& url) {
    content::AccessibilityNotificationWaiter waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    ASSERT_TRUE(waiter.WaitForNotification());

    autoclick_test_utils_->LoadAutoclick();
  }

  ui::test::EventGenerator* generator() { return generator_.get(); }
  AutoclickTestUtils* utils() { return autoclick_test_utils_.get(); }

 private:
  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<AutoclickTestUtils> autoclick_test_utils_;
};

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, LeftClickButtonOnHover) {
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");
  // No need to change click type: Default should be right-click.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  utils()->HoverOverHtmlElement(GetWebContents(), generator(), "test_button");
  tab_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, DoubleClickHover) {
  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <input type="text" id="text_field"
             value="peanutbuttersandwichmadewithjam">
      )");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kDoubleClick);

  content::AccessibilityNotificationWaiter selection_waiter(
      browser()->tab_strip_model()->GetActiveWebContents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  content::BoundingBoxUpdateWaiter bounding_box_waiter(GetWebContents());

  // Double-clicking over the text field should result in the text being
  // selected.
  utils()->HoverOverHtmlElement(GetWebContents(), generator(), "text_field");

  bounding_box_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, ClickAndDrag) {
  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <input type="text" id="text_field"
             value="peanutbuttersandwichmadewithjam">
      )");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kDragAndDrop);

  const gfx::Rect bounds =
      GetControlBoundsInRoot(GetWebContents(), "text_field");

  content::AccessibilityNotificationWaiter selection_waiter(
      browser()->tab_strip_model()->GetActiveWebContents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);

  // First hover causes a down click even that changes the caret.
  CaretBoundsChangedWaiter caret_waiter(
      browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod());
  generator()->MoveMouseTo(
      gfx::Point(bounds.left_center().y(), bounds.x() + 10));
  caret_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  // Second hover causes a selection.
  content::BoundingBoxUpdateWaiter bounding_box_waiter(GetWebContents());
  generator()->MoveMouseTo(bounds.right_center());
  bounding_box_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest,
                       RightClickOnHoverOpensContextMenu) {
  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <input type="text" id="text_field" value="stop copying me">
      )");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kRightClick);

  ContextMenuWaiter context_menu_waiter;

  // Right clicking over the text field should result in a context menu.
  utils()->HoverOverHtmlElement(GetWebContents(), generator(), "text_field");

  context_menu_waiter.WaitForMenuOpenAndClose();

  // Since we right-clicked on a context menu, the copy/paste commands were
  // included.
  EXPECT_THAT(context_menu_waiter.GetCapturedCommandIds(),
              testing::IsSupersetOf(
                  {IDC_CONTENT_CONTEXT_COPY, IDC_CONTENT_CONTEXT_PASTE}));
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest,
                       ScrollHoverHighlightsScrollableArea) {
  utils()->ObserveFocusRings();

  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <textarea id="test_textarea" rows="2" cols="20">"Whatever you
          choose to do, leave tracks. That means don't do it just for
          yourself. You will want to leave the world a little better
          for your havinglived."</textarea>
      )");

  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  std::string focus_ring_id = AccessibilityManager::Get()->GetFocusRingId(
      extension_misc::kAccessibilityCommonExtensionId, "");
  const AccessibilityFocusRingGroup* focus_ring_group =
      controller->GetFocusRingGroupForTesting(focus_ring_id);
  // No focus rings to start.
  EXPECT_EQ(nullptr, focus_ring_group);

  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kScroll);

  utils()->HoverOverHtmlElement(GetWebContents(), generator(), "test_textarea");
  utils()->WaitForFocusRingChanged();

  focus_ring_group = controller->GetFocusRingGroupForTesting(focus_ring_id);
  ASSERT_NE(nullptr, focus_ring_group);
  std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> const& focus_rings =
      focus_ring_group->focus_layers_for_testing();
  ASSERT_EQ(focus_rings.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, LongDelay) {
  utils()->SetAutoclickDelayMs(500);
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  base::ElapsedTimer timer;
  utils()->HoverOverHtmlElement(GetWebContents(), generator(), "test_button");
  tab_waiter.Wait();
  EXPECT_GT(timer.Elapsed().InMilliseconds(), 500);
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, PauseAutoclick) {
  utils()->SetAutoclickDelayMs(5);
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");
  utils()->SetAutoclickEventTypeWithHover(generator(),
                                          AutoclickEventType::kNoAction);

  base::OneShotTimer timer;
  base::RunLoop runner;
  utils()->HoverOverHtmlElement(GetWebContents(), generator(), "test_button");
  timer.Start(FROM_HERE, base::Milliseconds(500),
              base::BindLambdaForTesting([&runner, this]() {
                runner.Quit();
                // Because the test above passes, we know that this would have
                // resulted in an action before 500 ms if autoclick was not
                // paused.
                EXPECT_EQ(1, browser()->tab_strip_model()->GetTabCount());
              }));
  runner.Run();
}

}  // namespace ash
