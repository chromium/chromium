// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/system/accessibility/autoclick_menu_view.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
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
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/test/event_generator.h"
#include "url/url_constants.h"

namespace ash {

class AutoclickBrowserTest : public InProcessBrowserTest {
 public:
  AutoclickBrowserTest(const AutoclickBrowserTest&) = delete;
  AutoclickBrowserTest& operator=(const AutoclickBrowserTest&) = delete;

  void OnFocusRingChanged() {
    if (loop_runner_ && loop_runner_->running()) {
      loop_runner_->Quit();
    }
  }

 protected:
  AutoclickBrowserTest() = default;
  ~AutoclickBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    ASSERT_FALSE(AccessibilityManager::Get()->IsAutoclickEnabled());
    console_observer_ = std::make_unique<ExtensionConsoleErrorObserver>(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);

    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);

    SetAutoclickDelayMs(5);

    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(browser()->profile()->GetPrefs());
    pref_change_registrar_->Add(
        prefs::kAccessibilityAutoclickEventType,
        base::BindRepeating(&AutoclickBrowserTest::OnEventTypePrefChanged,
                            GetWeakPtr()));

    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  void TearDownOnMainThread() override { pref_change_registrar_.reset(); }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // Loads a page with the given URL and then starts up Autoclick.
  void LoadURLAndAutoclick(const std::string& url) {
    content::AccessibilityNotificationWaiter waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    ASSERT_TRUE(waiter.WaitForNotification());

    extensions::ExtensionHostTestHelper host_helper(
        browser()->profile(), extension_misc::kAccessibilityCommonExtensionId);
    AccessibilityManager::Get()->EnableAutoclick(true);
    Shell::Get()
        ->autoclick_controller()
        ->GetMenuBubbleControllerForTesting()
        ->SetAnimateForTesting(false);
    host_helper.WaitForHostCompletedFirstLoad();
    WaitForAutoclickReady();
  }

  void WaitForAutoclickReady() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string script = base::StringPrintf(R"JS(
      (async function() {
        window.accessibilityCommon.setFeatureLoadCallbackForTest('autoclick',
            () => {
              window.domAutomationController.send('ready');
            });
      })();
    )JS");
    std::string result =
        extensions::browsertest_util::ExecuteScriptInBackgroundPage(
            browser()->profile(),
            extension_misc::kAccessibilityCommonExtensionId, script);
    ASSERT_EQ("ready", result);
  }

  void SetAutoclickDelayMs(int ms) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(prefs::kAccessibilityAutoclickDelayMs, ms);
  }

  void OnEventTypePrefChanged() {
    if (pref_change_waiter_)
      std::move(pref_change_waiter_).Run();
  }

  // Performs a hover over the autoclick menu to change the event type.
  void SetAutoclickEventType(AutoclickEventType type) {
    // Check if we already have the right type selected.
    PrefService* prefs = browser()->profile()->GetPrefs();
    if (prefs->GetInteger(prefs::kAccessibilityAutoclickEventType) ==
        static_cast<int>(type)) {
      return;
    }

    // Find the menu button.
    AutoclickMenuView::ButtonId button_id;
    switch (type) {
      case AutoclickEventType::kLeftClick:
        button_id = AutoclickMenuView::ButtonId::kLeftClick;
        break;
      case AutoclickEventType::kRightClick:
        button_id = AutoclickMenuView::ButtonId::kRightClick;
        break;
      case AutoclickEventType::kDoubleClick:
        button_id = AutoclickMenuView::ButtonId::kDoubleClick;
        break;
      case AutoclickEventType::kDragAndDrop:
        button_id = AutoclickMenuView::ButtonId::kDragAndDrop;
        break;
      case AutoclickEventType::kScroll:
        button_id = AutoclickMenuView::ButtonId::kScroll;
        break;
      case AutoclickEventType::kNoAction:
        button_id = AutoclickMenuView::ButtonId::kPause;
        break;
    }
    AutoclickMenuView* menu_view = Shell::Get()
                                       ->autoclick_controller()
                                       ->GetMenuBubbleControllerForTesting()
                                       ->menu_view_;
    ASSERT_NE(nullptr, menu_view);
    auto* button_view = menu_view->GetViewByID(static_cast<int>(button_id));
    ASSERT_NE(nullptr, button_view);

    // Hover over it.
    const gfx::Rect bounds = button_view->GetBoundsInScreen();
    generator_->MoveMouseTo(bounds.CenterPoint());

    // Wait for the pref change, indicating the button was pressed.
    base::RunLoop runner;
    pref_change_waiter_ = runner.QuitClosure();
    runner.Run();
  }

  void HoverOverHtmlElement(const std::string& element) {
    const gfx::Rect bounds = GetControlBoundsInRoot(GetWebContents(), element);
    generator_->MoveMouseTo(bounds.CenterPoint());
  }

  void WaitForFocusRingChanged() {
    loop_runner_ = std::make_unique<base::RunLoop>();
    loop_runner_->Run();
  }

  base::WeakPtr<AutoclickBrowserTest> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;

 private:
  std::unique_ptr<ExtensionConsoleErrorObserver> console_observer_;
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;
  base::OnceClosure pref_change_waiter_;
  std::unique_ptr<base::RunLoop> loop_runner_;
  base::WeakPtrFactory<AutoclickBrowserTest> weak_ptr_factory_{this};
};

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, LeftClickButtonOnHover) {
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");
  // No need to change click type: Default should be right-click.
  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  HoverOverHtmlElement("test_button");
  tab_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, DoubleClickHover) {
  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <input type="text" id="text_field"
             value="peanutbuttersandwichmadewithjam">
      )");
  SetAutoclickEventType(AutoclickEventType::kDoubleClick);

  content::AccessibilityNotificationWaiter selection_waiter(
      browser()->tab_strip_model()->GetActiveWebContents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  content::BoundingBoxUpdateWaiter bounding_box_waiter(GetWebContents());

  // Double-clicking over the text field should result in the text being
  // selected.
  HoverOverHtmlElement("text_field");

  bounding_box_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, ClickAndDrag) {
  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <input type="text" id="text_field"
             value="peanutbuttersandwichmadewithjam">
      )");
  SetAutoclickEventType(AutoclickEventType::kDragAndDrop);

  const gfx::Rect bounds =
      GetControlBoundsInRoot(GetWebContents(), "text_field");

  content::AccessibilityNotificationWaiter selection_waiter(
      browser()->tab_strip_model()->GetActiveWebContents(), ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);

  // First hover causes a down click even that changes the caret.
  CaretBoundsChangedWaiter caret_waiter(
      browser()->window()->GetNativeWindow()->GetHost()->GetInputMethod());
  generator_->MoveMouseTo(
      gfx::Point(bounds.left_center().y(), bounds.x() + 10));
  caret_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());

  // Second hover causes a selection.
  content::BoundingBoxUpdateWaiter bounding_box_waiter(GetWebContents());
  generator_->MoveMouseTo(bounds.right_center());
  bounding_box_waiter.Wait();
  ASSERT_TRUE(selection_waiter.WaitForNotification());
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest,
                       RightClickOnHoverOpensContextMenu) {
  LoadURLAndAutoclick(R"(
      data:text/html;charset=utf-8,
      <input type="text" id="text_field" value="stop copying me">
      )");
  SetAutoclickEventType(AutoclickEventType::kRightClick);

  ContextMenuWaiter context_menu_waiter;

  // Right clicking over the text field should result in a context menu.
  HoverOverHtmlElement("text_field");

  context_menu_waiter.WaitForMenuOpenAndClose();

  // Since we right-clicked on a context menu, the copy/paste commands were
  // included.
  EXPECT_THAT(context_menu_waiter.GetCapturedCommandIds(),
              testing::IsSupersetOf(
                  {IDC_CONTENT_CONTEXT_COPY, IDC_CONTENT_CONTEXT_PASTE}));
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest,
                       ScrollHoverHighlightsScrollableArea) {
  // Create a callback for the focus ring observer.
  AccessibilityManager::Get()->SetFocusRingObserverForTest(base::BindRepeating(
      &AutoclickBrowserTest::OnFocusRingChanged, GetWeakPtr()));

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

  SetAutoclickEventType(AutoclickEventType::kScroll);

  HoverOverHtmlElement("test_textarea");
  WaitForFocusRingChanged();

  focus_ring_group = controller->GetFocusRingGroupForTesting(focus_ring_id);
  ASSERT_NE(nullptr, focus_ring_group);
  std::vector<std::unique_ptr<AccessibilityFocusRingLayer>> const& focus_rings =
      focus_ring_group->focus_layers_for_testing();
  ASSERT_EQ(focus_rings.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, LongDelay) {
  SetAutoclickDelayMs(500);
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  base::ElapsedTimer timer;
  HoverOverHtmlElement("test_button");
  tab_waiter.Wait();
  EXPECT_GT(timer.Elapsed().InMilliseconds(), 500);
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, ShortDelay) {
  SetAutoclickDelayMs(5);
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");

  ui_test_utils::TabAddedWaiter tab_waiter(browser());
  base::ElapsedTimer timer;
  HoverOverHtmlElement("test_button");
  tab_waiter.Wait();
  // Seems to take around 100 ms, so let's check 500 ms to be safe.
  EXPECT_LT(timer.Elapsed().InMilliseconds(), 500);
  EXPECT_EQ(2, browser()->tab_strip_model()->GetTabCount());
}

IN_PROC_BROWSER_TEST_F(AutoclickBrowserTest, PauseAutoclick) {
  SetAutoclickDelayMs(5);
  LoadURLAndAutoclick(R"(
        data:text/html;charset=utf-8,
        <input type="button" id="test_button"
               onclick="window.open();" value="click me">
      )");
  SetAutoclickEventType(AutoclickEventType::kNoAction);

  base::OneShotTimer timer;
  base::RunLoop runner;
  HoverOverHtmlElement("test_button");
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
