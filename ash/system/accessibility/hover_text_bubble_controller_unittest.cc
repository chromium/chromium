// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/hover_text_bubble_controller.h"

#include <algorithm>
#include <memory>
#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/system/accessibility/hover_text_bubble_view.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "base/test/scoped_feature_list.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/aura/window.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace ash {

class HoverTextBubbleControllerTest : public AshTestBase {
 public:
  HoverTextBubbleControllerTest() = default;
  HoverTextBubbleControllerTest(const HoverTextBubbleControllerTest&) = delete;
  HoverTextBubbleControllerTest& operator=(
      const HoverTextBubbleControllerTest&) = delete;
  ~HoverTextBubbleControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityHoverText);
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->hover_text().SetEnabled(true);
  }

  HoverTextBubbleController* GetController() {
    return Shell::Get()
        ->accessibility_controller()
        ->GetHoverTextBubbleControllerForTest();
  }

  HoverTextBubbleView* GetView() {
    return GetController() ? GetController()->bubble_view_for_testing()
                           : nullptr;
  }

  views::Widget* GetWidget() {
    return GetController() ? GetController()->widget_for_testing() : nullptr;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HoverTextBubbleControllerTest, ShowUpdateAndHideLifecycle) {
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);
  EXPECT_FALSE(controller->IsBubbleVisibleForTesting());

  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Downloads folder";
  props.anchor_rect = gfx::Rect(260, 280, 80, 40);
  props.font_scale = 2.0;
  props.font_style = HoverTextFontStyle::kSystemSans;
  controller->UpdateBubble(props);

  EXPECT_TRUE(controller->IsBubbleVisibleForTesting());
  EXPECT_EQ(u"Downloads folder", controller->GetBubbleTextForTesting());

  // Hiding the bubble keeps the widget alive and marks it not visible.
  HoverTextBubbleProps hide_props;
  hide_props.visible = false;
  controller->UpdateBubble(hide_props);
  EXPECT_FALSE(controller->IsBubbleVisibleForTesting());
}

TEST_F(HoverTextBubbleControllerTest, FontScalingAndFontStyleApplication) {
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Scaled text";
  props.anchor_rect = gfx::Rect(220, 230, 60, 40);
  props.font_scale = 1.5;
  props.font_style = HoverTextFontStyle::kSystemSans;
  controller->UpdateBubble(props);

  ASSERT_TRUE(GetView());
  EXPECT_EQ(24, GetView()->GetFontSizePixelsForTesting());
  const std::string sans_family = GetView()->GetFontFamilyForTesting();
  EXPECT_TRUE(sans_family == "Roboto" || sans_family == "DejaVu Sans" ||
              sans_family == "sans-serif");
  EXPECT_EQ(sans_family,
            GetView()->GetFontListForTesting().GetPrimaryFont().GetFontName());
  const std::string sans_actual =
      GetView()->GetFontListForTesting().GetPrimaryFont().GetActualFontName();

  // Updating font scale and style via `UpdateBubble` updates the visible
  // bubble (16px * 3.0 = 48px).
  props.font_scale = 3.0;
  props.font_style = HoverTextFontStyle::kMonospace;
  controller->UpdateBubble(props);
  EXPECT_EQ(48, GetView()->GetFontSizePixelsForTesting());
  const std::string mono_family = GetView()->GetFontFamilyForTesting();
  EXPECT_TRUE(mono_family == "Roboto Mono" ||
              mono_family == "DejaVu Sans Mono" || mono_family == "monospace");
  EXPECT_EQ(mono_family,
            GetView()->GetFontListForTesting().GetPrimaryFont().GetFontName());
  const std::string mono_actual =
      GetView()->GetFontListForTesting().GetPrimaryFont().GetActualFontName();
  EXPECT_NE(sans_family, mono_family);
  EXPECT_NE(sans_actual, mono_actual);

  // Out-of-range scales are clamped to [16px, 64px].
  props.font_scale = 6.0;
  props.font_style = HoverTextFontStyle::kSerif;
  controller->UpdateBubble(props);
  EXPECT_EQ(64, GetView()->GetFontSizePixelsForTesting());
  const std::string serif_family = GetView()->GetFontFamilyForTesting();
  EXPECT_TRUE(serif_family == "Noto Serif" || serif_family == "DejaVu Serif" ||
              serif_family == "serif");
  EXPECT_EQ(serif_family,
            GetView()->GetFontListForTesting().GetPrimaryFont().GetFontName());
  const std::string serif_actual =
      GetView()->GetFontListForTesting().GetPrimaryFont().GetActualFontName();
  EXPECT_NE(sans_family, serif_family);
  EXPECT_NE(mono_family, serif_family);
  EXPECT_NE(sans_actual, serif_actual);
  EXPECT_NE(mono_actual, serif_actual);
}

TEST_F(HoverTextBubbleControllerTest,
       ZeroAccessibilityPropertiesUnfocusableAndEventPassthrough) {
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Unfocusable bubble";
  props.anchor_rect = gfx::Rect(280, 280, 40, 40);
  controller->UpdateBubble(props);

  HoverTextBubbleView* view = GetView();
  views::Widget* widget = GetWidget();
  ASSERT_TRUE(view);
  ASSERT_TRUE(widget);

  // Verify the bubble view, its internal label, and the widget root view are
  // ignored and marked as leaves in the accessibility tree and cannot be
  // focused, clicked, or activated.
  EXPECT_TRUE(view->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(view->GetViewAccessibility().IsLeaf());
  EXPECT_EQ(views::View::FocusBehavior::NEVER, view->GetFocusBehavior());
  EXPECT_FALSE(view->GetCanProcessEventsWithinSubtree());
  EXPECT_FALSE(view->CanActivate());
  EXPECT_FALSE(widget->CanActivate());

  ASSERT_TRUE(view->label_for_testing());
  EXPECT_TRUE(view->label_for_testing()->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(view->label_for_testing()->GetViewAccessibility().IsLeaf());
  EXPECT_NE(props.text,
            view->label_for_testing()->GetViewAccessibility().GetCachedName());
  EXPECT_EQ(views::View::FocusBehavior::NEVER,
            view->label_for_testing()->GetFocusBehavior());

  ASSERT_TRUE(widget->GetRootView());
  EXPECT_EQ(ax::mojom::Role::kNone, view->GetAccessibleWindowRole());
  EXPECT_NE(ax::mojom::Role::kAlertDialog,
            widget->GetRootView()->GetViewAccessibility().GetCachedRole());
  EXPECT_FALSE(widget->GetRootView()->GetCanProcessEventsWithinSubtree());
  EXPECT_TRUE(widget->GetRootView()->GetViewAccessibility().GetIsIgnored());
  EXPECT_TRUE(widget->GetRootView()->GetViewAccessibility().IsLeaf());

  // Verify the native window is suppressed from `AutomationManagerAura`,
  // ignores pointer events, and allows a mouse click directly over the visible
  // bubble to pass transparently through to an underlying button without
  // activating the bubble.
  ASSERT_TRUE(widget->GetNativeWindow());
  EXPECT_EQ(aura::EventTargetingPolicy::kNone,
            widget->GetNativeWindow()->event_targeting_policy());
  EXPECT_TRUE(widget->GetNativeWindow()->GetProperty(
      ui::kAXConsiderInvisibleAndIgnoreChildren));
  EXPECT_EQ(ax::mojom::Role::kNone,
            widget->GetNativeWindow()->GetProperty(ui::kAXRoleOverride));

  const gfx::Rect bubble_bounds =
      controller->GetBubbleBoundsInScreenForTesting();
  ASSERT_FALSE(bubble_bounds.IsEmpty());

  int underlying_button_clicks = 0;
  std::unique_ptr<views::Widget> app_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  app_widget->SetBounds(bubble_bounds);
  views::LabelButton* button = app_widget->GetContentsView()->AddChildView(
      std::make_unique<views::LabelButton>(
          base::BindRepeating([](int* clicks) { ++(*clicks); },
                              &underlying_button_clicks),
          u"Underlying Button"));
  button->SetBoundsRect(gfx::Rect(bubble_bounds.size()));
  app_widget->Show();

  // Re-update the bubble so it stays stacked above `app_widget` while sharing
  // the same screen coordinates.
  controller->UpdateBubble(props);
  ASSERT_TRUE(controller->IsBubbleVisibleForTesting());

  GetEventGenerator()->MoveMouseTo(bubble_bounds.CenterPoint());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_EQ(1, underlying_button_clicks);
  EXPECT_FALSE(widget->IsActive());
  EXPECT_TRUE(controller->IsBubbleVisibleForTesting());
}

TEST_F(HoverTextBubbleControllerTest,
       BubbleShowUpdateAndHidePreservesActiveWindowAndTextfieldTyping) {
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  // Create an active application widget containing a focused `views::Textfield`
  // to verify that showing, updating, and hiding the Hover Text bubble never
  // deactivates the active window, steals view focus, or disrupts keyboard
  // typing.
  std::unique_ptr<views::Widget> app_widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  app_widget->SetBounds(gfx::Rect(100, 100, 300, 200));
  views::Textfield* textfield = app_widget->GetContentsView()->AddChildView(
      std::make_unique<views::Textfield>());
  textfield->SetBoundsRect(gfx::Rect(10, 10, 200, 30));
  app_widget->Activate();
  textfield->RequestFocus();

  ASSERT_TRUE(app_widget->IsActive());
  ASSERT_EQ(app_widget->GetNativeWindow(), window_util::GetActiveWindow());
  ASSERT_EQ(textfield, app_widget->GetFocusManager()->GetFocusedView());

  // Type while the bubble is hidden.
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_A);
  EXPECT_EQ(u"a", textfield->GetText());

  // Show the Hover Text bubble and verify the active window and textfield focus
  // remain unchanged while typing continues into `textfield`.
  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Search input";
  props.anchor_rect = gfx::Rect(110, 110, 200, 30);
  controller->UpdateBubble(props);

  EXPECT_TRUE(controller->IsBubbleVisibleForTesting());
  ASSERT_TRUE(GetWidget());
  EXPECT_FALSE(GetWidget()->IsActive());
  EXPECT_TRUE(app_widget->IsActive());
  EXPECT_EQ(app_widget->GetNativeWindow(), window_util::GetActiveWindow());
  EXPECT_EQ(textfield, app_widget->GetFocusManager()->GetFocusedView());

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_B);
  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_C);
  EXPECT_EQ(u"abc", textfield->GetText());

  // Update the already-visible bubble text and bounds and verify typing still
  // flows uninterrupted into `textfield`.
  props.text = u"Updated label";
  props.anchor_rect = gfx::Rect(120, 115, 200, 30);
  controller->UpdateBubble(props);

  EXPECT_TRUE(controller->IsBubbleVisibleForTesting());
  EXPECT_FALSE(GetWidget()->IsActive());
  EXPECT_TRUE(app_widget->IsActive());
  EXPECT_EQ(app_widget->GetNativeWindow(), window_util::GetActiveWindow());
  EXPECT_EQ(textfield, app_widget->GetFocusManager()->GetFocusedView());

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_D);
  EXPECT_EQ(u"abcd", textfield->GetText());

  // Hide the bubble and verify focus and typing remain intact.
  controller->Hide();
  EXPECT_FALSE(controller->IsBubbleVisibleForTesting());
  EXPECT_TRUE(app_widget->IsActive());
  EXPECT_EQ(app_widget->GetNativeWindow(), window_util::GetActiveWindow());
  EXPECT_EQ(textfield, app_widget->GetFocusManager()->GetFocusedView());

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_E);
  EXPECT_EQ(u"abcde", textfield->GetText());
}

TEST_F(HoverTextBubbleControllerTest,
       PlacementFlipsAboveNearBottomEdgeAndStaysInScreen) {
  UpdateDisplay("800x600");
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  // Target in the middle of the screen: bubble should be placed below target.
  HoverTextBubbleProps middle_props;
  middle_props.visible = true;
  middle_props.text = u"Middle target";
  middle_props.anchor_rect = gfx::Rect(360, 200, 80, 40);
  controller->UpdateBubble(middle_props);
  gfx::Rect middle_bubble_bounds =
      controller->GetBubbleBoundsInScreenForTesting();
  EXPECT_GE(middle_bubble_bounds.y(), middle_props.anchor_rect.bottom());

  // Target near the bottom shelf of the screen: bubble should flip above
  // target so it does not intersect `anchor_rect`.
  HoverTextBubbleProps bottom_props;
  bottom_props.visible = true;
  bottom_props.text = u"Bottom edge target";
  bottom_props.anchor_rect = gfx::Rect(360, 520, 80, 40);
  controller->UpdateBubble(bottom_props);
  gfx::Rect bottom_bubble_bounds =
      controller->GetBubbleBoundsInScreenForTesting();
  EXPECT_LE(bottom_bubble_bounds.bottom(), bottom_props.anchor_rect.y());
}

TEST_F(HoverTextBubbleControllerTest,
       RecreatesBubbleAfterNativeWindowDestroyed) {
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Initial bubble";
  props.anchor_rect = gfx::Rect(260, 280, 80, 40);
  controller->UpdateBubble(props);
  ASSERT_TRUE(controller->IsBubbleVisibleForTesting());
  ASSERT_TRUE(GetWidget());
  ASSERT_TRUE(GetWidget()->GetNativeWindow());

  // Simulate external native window destruction (e.g., display teardown) which
  // resets both `bubble_view_` and `widget_` synchronously without leaving a
  // zombie `views::Widget`.
  delete GetWidget()->GetNativeWindow();
  EXPECT_FALSE(GetView());
  EXPECT_FALSE(GetWidget());
  EXPECT_FALSE(controller->IsBubbleVisibleForTesting());

  // A subsequent `UpdateBubble()` call must safely recreate the widget and
  // `HoverTextBubbleView` rather than dereferencing a null `bubble_view_`.
  props.text = u"Recreated bubble";
  controller->UpdateBubble(props);
  EXPECT_TRUE(controller->IsBubbleVisibleForTesting());
  EXPECT_EQ(u"Recreated bubble", controller->GetBubbleTextForTesting());
}

TEST_F(HoverTextBubbleControllerTest,
       ReparentsToTargetDisplayContainerAcrossDisplays) {
  UpdateDisplay("800x600,800x600");
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  aura::Window* primary_container = Shell::GetContainer(
      root_windows[0], kShellWindowId_AccessibilityBubbleContainer);
  aura::Window* secondary_container = Shell::GetContainer(
      root_windows[1], kShellWindowId_AccessibilityBubbleContainer);
  ASSERT_TRUE(primary_container);
  ASSERT_TRUE(secondary_container);

  // Show on the primary display (x in [0, 800)).
  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Primary display";
  props.anchor_rect = gfx::Rect(200, 200, 80, 40);
  controller->UpdateBubble(props);

  ASSERT_TRUE(GetWidget());
  aura::Window* bubble_window = GetWidget()->GetNativeWindow();
  ASSERT_TRUE(bubble_window);
  EXPECT_EQ(primary_container, bubble_window->parent());
  EXPECT_TRUE(
      std::ranges::contains(primary_container->children(), bubble_window));
  EXPECT_FALSE(
      std::ranges::contains(secondary_container->children(), bubble_window));

  // Move the anchor to the secondary display (x in [800, 1600)) and verify the
  // bubble window is reparented to `secondary_container` and removed from
  // `primary_container`.
  props.text = u"Secondary display";
  props.anchor_rect = gfx::Rect(1000, 200, 80, 40);
  controller->UpdateBubble(props);

  EXPECT_EQ(secondary_container, bubble_window->parent());
  EXPECT_FALSE(
      std::ranges::contains(primary_container->children(), bubble_window));
  EXPECT_TRUE(
      std::ranges::contains(secondary_container->children(), bubble_window));
}

using HoverTextBubbleViewTest = HoverTextBubbleControllerTest;

TEST_F(HoverTextBubbleViewTest, DirectUpdateWrappingElisionAndSizing) {
  HoverTextBubbleController* controller = GetController();
  ASSERT_TRUE(controller);

  HoverTextBubbleProps props;
  props.visible = true;
  props.text = u"Short label";
  props.anchor_rect = gfx::Rect(200, 200, 80, 40);
  controller->UpdateBubble(props);

  HoverTextBubbleView* view = GetView();
  ASSERT_TRUE(view);
  ASSERT_TRUE(view->label_for_testing());

  // Direct `HoverTextBubbleView::Update()` with a short string keeps
  // single-line layout (`GetMultiLine() == false`) and shrinks below the 480
  // DIP cap.
  view->Update(u"Brief text", /*font_scale=*/1.5,
               HoverTextFontStyle::kSystemSans);
  EXPECT_EQ(u"Brief text", view->GetTextForTesting());
  EXPECT_EQ(24, view->GetFontSizePixelsForTesting());
  EXPECT_FALSE(view->label_for_testing()->GetMultiLine());
  const int short_width = view->GetPreferredSize().width();
  EXPECT_LT(short_width, 480 + 32);

  // Direct `HoverTextBubbleView::Update()` with a long string enables
  // multi-line wrapping (`GetMultiLine() == true`), caps max lines at 4, and
  // sets `gfx::ELIDE_TAIL`.
  const std::u16string long_text(
      300, u'W');  // Exceeds 480 DIP single-line width at 32px font size.
  view->Update(long_text, /*font_scale=*/2.0, HoverTextFontStyle::kMonospace);
  EXPECT_EQ(long_text, view->GetTextForTesting());
  EXPECT_EQ(32, view->GetFontSizePixelsForTesting());
  EXPECT_TRUE(view->label_for_testing()->GetMultiLine());
  EXPECT_EQ(4u, view->label_for_testing()->GetMaxLines());
  EXPECT_EQ(gfx::ELIDE_TAIL, view->label_for_testing()->GetElideBehavior());
  EXPECT_GT(view->GetPreferredSize().width(), short_width);
}

}  // namespace ash
