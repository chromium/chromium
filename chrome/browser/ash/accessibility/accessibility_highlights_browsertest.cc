// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "build/build_config.h"

#include "ash/accessibility/ui/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/ui/accessibility_focus_ring_controller_impl.h"
#include "ash/accessibility/ui/accessibility_focus_ring_layer.h"
#include "ash/accessibility/ui/accessibility_highlight_layer.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "chrome/browser/ash/accessibility/accessibility_feature_browsertest.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/automation_test_utils.h"
#include "chrome/browser/ash/accessibility/select_to_speak_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "ui/compositor/layer.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

class AccessibilityHighlightsBrowserTest
    : public AccessibilityFeatureBrowserTest {
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
    AccessibilityFeatureBrowserTest::SetUpOnMainThread();

    // Load Select to Speak so we have a Javascript context with access to the
    // Automation API to inject AutomationTestUtils. Select to Speak doesn't do
    // any work unless it is triggered, so this does not impact the test.
    sts_test_utils::TurnOnSelectToSpeakForTest(GetProfile());
    utils_ = std::make_unique<AutomationTestUtils>(
        extension_misc::kSelectToSpeakExtensionId);
    utils_->SetUpTestSupport();

    NavigateToUrl(GURL(url::kAboutBlankURL));
  }

  void OnFocusRingsChanged() {
    if (focus_ring_waiter_) {
      std::move(focus_ring_waiter_).Run();
    }
  }

  void WaitForFocusRingsChanged() {
    base::RunLoop run_loop;
    focus_ring_waiter_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::unique_ptr<ui::test::EventGenerator> generator_;
  std::unique_ptr<AutomationTestUtils> utils_;

 private:
  base::OnceClosure focus_ring_waiter_;
};

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest,
                       CursorHighlightAddsFocusRing) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  EXPECT_FALSE(controller->cursor_layer_for_testing());

  PrefService* prefs = GetProfile()->GetPrefs();
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

  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, true);

  // Still doesn't exist because no input text area is focused.
  EXPECT_FALSE(controller->caret_layer_for_testing());

  const struct {
    std::string url;
    std::string name;
    std::string role;
  } kTestCases[] = {{"data:text/html;charset=utf-8,"
                     "<textarea>Hello there</textarea>",
                     "Hello there", "staticText"},
                    {"data:text/html;charset=utf-8,"
                     "<input type=\"text\" value=\"Hows it going?\">",
                     "Hows it going?", "staticText"},
                    {"data:text/html;charset=utf-8,"
                     "<div contenteditable=\"true\">"
                     "<p>Not bad, and <b>you</b>?</p>"
                     "</div>",
                     "Not bad, and ", "staticText"}};

  for (const auto& testCase : kTestCases) {
    NavigateToUrl(GURL(testCase.url));
    gfx::Rect element_bounds =
        utils_->GetNodeBoundsInRoot(testCase.name, testCase.role);

    generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
    WaitForFocusRingsChanged();
    AccessibilityCursorRingLayer* caret_layer =
        controller->caret_layer_for_testing();
    while (caret_layer == nullptr) {
      WaitForFocusRingsChanged();
      caret_layer = controller->caret_layer_for_testing();
    }
    ASSERT_TRUE(caret_layer);
    gfx::Rect initial_bounds = caret_layer->layer()->GetTargetBounds();
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

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest,
                       CaretHighlightOmnibox) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, true);

  // Will wait for the omnibox to be shown.
  const gfx::Rect omnibox_bounds =
      utils_->GetBoundsForNodeInRootByClassName("OmniboxViewViews");

  // Jump to the omnibox.
  generator_->PressAndReleaseKeyAndModifierKeys(ui::KeyboardCode::VKEY_L,
                                                ui::EF_CONTROL_DOWN);
  WaitForFocusRingsChanged();
  AccessibilityCursorRingLayer* caret_layer =
      controller->caret_layer_for_testing();
  ASSERT_TRUE(caret_layer);
  gfx::Rect bounds = caret_layer->layer()->GetTargetBounds();
  EXPECT_EQ(bounds.CenterPoint().y(), omnibox_bounds.CenterPoint().y());

  // On the left edge of the omnibox.
  EXPECT_LT(bounds.x(), omnibox_bounds.x());
  EXPECT_GT(bounds.right(), omnibox_bounds.x());

  // Typing something shifts the bounds to the right.
  generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_K);
  gfx::Rect new_bounds = caret_layer->layer()->GetTargetBounds();
  if (new_bounds == bounds) {
    WaitForFocusRingsChanged();
    new_bounds = caret_layer->layer()->GetTargetBounds();
  }
  EXPECT_EQ(bounds.y(), new_bounds.y());
  EXPECT_LT(bounds.x(), new_bounds.x());

  prefs->SetBoolean(prefs::kAccessibilityCaretHighlightEnabled, false);
}

IN_PROC_BROWSER_TEST_F(AccessibilityHighlightsBrowserTest, FocusHighlight) {
  AccessibilityFocusRingControllerImpl* controller =
      Shell::Get()->accessibility_focus_ring_controller();
  PrefService* prefs = GetProfile()->GetPrefs();
  prefs->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled, true);

  const std::string url =
      "data:text/html;charset=utf-8,"
      "<input type=\"text\" value=\"long enough text to fill a whole textbox\">"
      "<input type=\"checkbox\" id=\"focus2\"><label for=\"focus2\">pick "
      "me</label>"
      "<input type=\"radio\" id=\"focus3\"><label for=\"focus3\">radio "
      "me</label>"
      "<input type=\"submit\">"
      "<a href=\"\">link</a>";
  NavigateToUrl(GURL(url));

  const struct {
    std::string name;
    std::string role;
  } kTestCases[] = {{"long enough text to fill a whole textbox", "staticText"},
                    {"pick me", "checkBox"},
                    {"radio me", "radioButton"},
                    {"Submit", "button"},
                    {"link", "link"}};

  for (const auto& testCase : kTestCases) {
    // Waits for the page to be loaded.
    gfx::Rect element_bounds =
        utils_->GetNodeBoundsInRoot(testCase.name, testCase.role);

    generator_->PressAndReleaseKey(ui::KeyboardCode::VKEY_TAB);
    WaitForFocusRingsChanged();

    const AccessibilityFocusRingGroup* highlights =
        controller->GetFocusRingGroupForTesting("HighlightController");
    ASSERT_TRUE(highlights);
    auto& focus_rings = highlights->focus_layers_for_testing();
    EXPECT_EQ(focus_rings.size(), 1u);
    gfx::Rect focus_bounds = focus_rings.at(0)->layer()->GetTargetBounds();
    if (testCase.role == "staticText") {
      // In the case of the text input field, the static text node we've
      // found bounds for within the input field is slightly shorter than its
      // parent to visually show that it will scroll. Check the center points
      // are within one pixel of each other.
      EXPECT_LT(
          (element_bounds.CenterPoint() - focus_bounds.CenterPoint()).Length(),
          2);
    } else {
      EXPECT_EQ(element_bounds.CenterPoint(), focus_bounds.CenterPoint());
    }
    EXPECT_TRUE(focus_bounds.Contains(element_bounds));
  }

  prefs->SetBoolean(prefs::kAccessibilityFocusHighlightEnabled, false);
}

}  // namespace ash
