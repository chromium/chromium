// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_highlight_layer.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/test/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/html_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class AccessibilityHighlightsBrowserTest : public InProcessBrowserTest {
 public:
  AccessibilityHighlightsBrowserTest(
      const AccessibilityHighlightsBrowserTest&) = delete;
  AccessibilityHighlightsBrowserTest& operator=(
      const AccessibilityHighlightsBrowserTest&) = delete;

 protected:
  AccessibilityHighlightsBrowserTest() = default;
  ~AccessibilityHighlightsBrowserTest() override = default;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override {
    aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
    generator_ = std::make_unique<ui::test::EventGenerator>(root_window);
    AccessibilityManager::Get()->SetFocusRingObserverForTest(
        base::BindRepeating(
            &AccessibilityHighlightsBrowserTest::OnFocusRingsChanged,
            base::Unretained(this)));
    Shell::Get()->accessibility_focus_ring_controller()->SetNoFadeForTesting();
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL)));
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void OnFocusRingsChanged() {
    if (focus_ring_waiter_)
      std::move(focus_ring_waiter_).Run();
  }

  void WaitForFocusRingsChanged() {
    base::RunLoop run_loop;
    focus_ring_waiter_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;
  base::OnceClosure focus_ring_waiter_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest,
                       CursorHighlightAddsFocusRing) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_FALSE(controller->cursor_layer_for_testing());

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityCursorHighlightEnabled, true);

  gfx::Point mouse_location(100, 100);
  generator_->MoveMouseTo(mouse_location);
  AccessibilityCursorRingLayer* cursor_layer =
      controller->cursor_layer_for_testing();
  ASSERT_TRUE(cursor_layer);
  gfx::Rect bounds = cursor_layer->layer()->GetTargetBounds();
  EXPECT_EQ(bounds.CenterPoint(), mouse_location);

  mouse_location = gfx::Point(200, 100);
  generator_->MoveMouseTo(mouse_location);
  bounds = cursor_layer->layer()->GetTargetBounds();
  EXPECT_EQ(bounds.CenterPoint(), mouse_location);

  // Turns off again.
  prefs->SetBoolean(prefs::kAccessibilityCursorHighlightEnabled, false);
  EXPECT_FALSE(controller->cursor_layer_for_testing());
}

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest,
                       CaretHighlightWebContents) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_FALSE(controller->caret_layer_for_testing());

  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, true);

  // Still doesn't exist because no input text area is focused.
  EXPECT_FALSE(controller->caret_layer_for_testing());

  const std::string kTestCases[] = {
      R"(data:text/html;charset=utf-8,
      <textarea id="field">Hello there</textarea>
    )",
      R"(data:text/html;charset=utf-8,
      <input id="field" type="text" value="How's it going?">
    )",
      R"(data:text/html;charset=utf-8,
      <div id="field" contenteditable="true">
        <p>Not bad, and <b>you</b>?</p>
      </div>
    )",
  };

  for (const auto& url : kTestCases) {
    content::AccessibilityNotificationWaiter waiter(
        GetWebContents(), ui::kAXModeComplete, ax::mojom::Event::kLoadComplete);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
    std::ignore = waiter.WaitForNotification();

    generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
    WaitForFocusRingsChanged();
    AccessibilityCursorRingLayer* caret_layer =
        controller->caret_layer_for_testing();
    ASSERT_TRUE(caret_layer);
    gfx::Rect initial_bounds = caret_layer->layer()->GetTargetBounds();

    gfx::Rect element_bounds =
        GetControlBoundsInRoot(GetWebContents(), "field");
    EXPECT_TRUE(element_bounds.Contains(initial_bounds.CenterPoint()));

    // Right arrow shifts the bounds to the right slightly.
    generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_RIGHT);
    WaitForFocusRingsChanged();
    gfx::Rect new_bounds = caret_layer->layer()->GetTargetBounds();
    EXPECT_EQ(initial_bounds.y(), new_bounds.y());
    EXPECT_LT(initial_bounds.x(), new_bounds.x());

    // Typing something shifts the bounds to the right also.
    generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_A);
    WaitForFocusRingsChanged();
    initial_bounds = new_bounds;
    new_bounds = caret_layer->layer()->GetTargetBounds();
    EXPECT_EQ(initial_bounds.y(), new_bounds.y());
    EXPECT_LT(initial_bounds.x(), new_bounds.x());
  }

  // Turns off again.
  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, false);
  EXPECT_FALSE(controller->caret_layer_for_testing());
}

// TODO(https://crbug.com/1453993): Failing in ChromeOS.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_CaretHighlightOmnibox DISABLED_CaretHighlightOmnibox
#else
#define MAYBE_CaretHighlightOmnibox CaretHighlightOmnibox
#endif
IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest,
                       MAYBE_CaretHighlightOmnibox) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, true);

  // Jump to the omnibox.
  generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_L, ui::EF_CONTROL_DOWN);
  WaitForFocusRingsChanged();
  AccessibilityCursorRingLayer* caret_layer =
      controller->caret_layer_for_testing();
  ASSERT_TRUE(caret_layer);
  gfx::Rect bounds = caret_layer->layer()->GetTargetBounds();

  const gfx::Rect omnibox_bounds =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetViewByID(VIEW_ID_OMNIBOX)
          ->GetBoundsInScreen();

  // TODO(crbug.com/1453711) Investigate why chromeOS is miscalculating the
  //   focus ring position in tests only. Visually, when running chromium, it
  //   looks right; the focus ring is centered in the omnibox. But when running
  //   tests, the focus ring is 1px higher than what it should be according to
  //   the code and visually.
  int expected_offset =
#if BUILDFLAG(IS_CHROMEOS)
      1;
#else
      0;
#endif
  EXPECT_TRUE(bounds.CenterPoint().y() ==
              omnibox_bounds.CenterPoint().y() - expected_offset);

  // On the left edge of the omnibox.
  EXPECT_LT(bounds.x(), omnibox_bounds.x());
  EXPECT_GT(bounds.right(), omnibox_bounds.x());

  // Typing something shifts the bounds to the right.
  generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_K);
  gfx::Rect new_bounds = caret_layer->layer()->GetTargetBounds();
  EXPECT_EQ(bounds.y(), new_bounds.y());
  EXPECT_LT(bounds.x(), new_bounds.x());
}

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest, FocusHighlight) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled, true);

  const std::string url = R"(
    data:text/html;charset=utf-8,
    <input type="text" id="focus1">
    <input type="checkbox" id="focus2">
    <input type="radio" id="focus3">
    <input type="submit" id="focus4">
    <a href="" id="focus5">link</a>
  )";
  content::AccessibilityNotificationWaiter waiter(
      browser()->tab_strip_model()->GetActiveWebContents(), ui::kAXModeComplete,
      ax::mojom::Event::kLoadComplete);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url)));
  std::ignore = waiter.WaitForNotification();

  const std::string kTestCases[] = {"focus1", "focus2", "focus3", "focus4",
                                    "focus5"};

  for (const auto& element : kTestCases) {
    generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
    WaitForFocusRingsChanged();
    const AccessibilityFocusRingGroup* highlights =
        controller->GetFocusRingGroupForTesting("HighlightController");
    ASSERT_TRUE(highlights);
    auto& focus_rings = highlights->focus_layers_for_testing();
    EXPECT_EQ(focus_rings.size(), 1u);
    gfx::Rect focus_bounds = focus_rings.at(0)->layer()->GetTargetBounds();
    gfx::Rect element_bounds =
        GetControlBoundsInRoot(GetWebContents(), element);
    EXPECT_EQ(element_bounds.CenterPoint(), focus_bounds.CenterPoint());
    EXPECT_TRUE(focus_bounds.Contains(element_bounds));
  }
}

}  // namespace ash
