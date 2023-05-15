// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_accessibility_controller.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/autoclick/autoclick_controller.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/ime_info.h"
#include "ash/public/cpp/session/session_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/accessibility/accessibility_detailed_view.h"
#include "ash/system/accessibility/autoclick_menu_bubble_controller.h"
#include "ash/system/accessibility/autoclick_menu_view.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/test/ash_test_base.h"
#include "base/barrier_closure.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

namespace {

// A buffer for checking whether the menu view is near this region of the
// screen. This buffer allows for things like padding and the shelf size,
// but is still smaller than half the screen size, so that we can check the
// general corner in which the menu is displayed.
const int kMenuViewBoundsBuffer = 100;
const char ImeEnglishId[] = "ime:english";

ui::GestureEvent CreateTapEvent() {
  return ui::GestureEvent(0, 0, 0, base::TimeTicks(),
                          ui::GestureEventDetails(ui::ET_GESTURE_TAP));
}

}  // namespace
class FloatingAccessibilityControllerTest : public AshTestBase {
 public:
  void SetUp() override {
    AshTestBase::SetUp();
    // Ensure 2 Ime's are available so we show the ime switch button.
    SetTwoAvailableImes();
  }

  AccessibilityControllerImpl* accessibility_controller() {
    return Shell::Get()->accessibility_controller();
  }

  void TearDown() override {
    AshTestBase::TearDown();
    features_.Reset();
  }

  FloatingAccessibilityController* controller() {
    return accessibility_controller()->GetFloatingMenuController();
  }

  FloatingMenuPosition menu_position() { return controller()->position_; }

  FloatingAccessibilityView* menu_view() {
    return controller() ? controller()->menu_view_.get() : nullptr;
  }

  views::Widget* widget() {
    return controller() ? controller()->bubble_widget_.get() : nullptr;
  }

  AutoclickMenuView* autoclick_menu_view() {
    AutoclickMenuBubbleController* controller =
        Shell::Get()
            ->autoclick_controller()
            ->GetMenuBubbleControllerForTesting();
    return controller ? controller->menu_view_.get() : nullptr;
  }

  bool detailed_view_shown() {
    return controller() && controller()->detailed_menu_controller_.get();
  }

  void WaitUntilAccessibilityTrayClosed() {
    // Waiting until accessibility tray is closed
    // after being notified from the observer.
    base::RunLoop run_loop;
    while (detailed_view_shown()) {
      run_loop.RunUntilIdle();
    }
  }

  views::View* GetMenuButton(FloatingAccessibilityView::ButtonId button_id) {
    FloatingAccessibilityView* view = menu_view();
    if (!view)
      return nullptr;
    return view->GetViewByID(static_cast<int>(button_id));
  }

  bool IsButtonVisible(FloatingAccessibilityView::ButtonId button_id) {
    views::View* button = GetMenuButton(button_id);
    return button != nullptr;
  }

  ImeMenuTray* GetImeTray() {
    ImeMenuTray* result =
        menu_view() ? menu_view()->ime_button_.get() : nullptr;
    EXPECT_NE(result, nullptr) << "Ime tray is not currently visible";
    return result;
  }

  TrayBackgroundView* GetVirtualKeyboardTray() {
    TrayBackgroundView* result =
        menu_view() ? menu_view()->virtual_keyboard_button_.get() : nullptr;
    EXPECT_NE(result, nullptr)
        << "Virtual keyboard tray is not currently visible";
    return result;
  }

  // Returns true if the IME menu bubble has been shown.
  bool IsImeTrayShown() { return GetImeTray()->GetBubbleView() != nullptr; }

  void SetUpKioskSession() {
    SessionInfo info;
    info.is_running_in_app_mode = true;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

  gfx::Rect GetMenuViewBounds() {
    FloatingAccessibilityView* view = menu_view();
    return view ? view->GetBoundsInScreen()
                : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

  gfx::Rect GetDetailedMenuBounds() {
    FloatingAccessibilityDetailedController* detailed_controller =
        controller() ? controller()->detailed_menu_controller_.get() : nullptr;
    return detailed_controller
               ? detailed_controller->detailed_view_->GetBoundsInScreen()
               : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

  gfx::Rect GetAutoclickMenuBounds() {
    return autoclick_menu_view()
               ? autoclick_menu_view()->GetBoundsInScreen()
               : gfx::Rect(-kMenuViewBoundsBuffer, -kMenuViewBoundsBuffer);
  }

  void Show() { accessibility_controller()->ShowFloatingMenuIfEnabled(); }

  void SetUpVisibleMenu() {
    SetUpKioskSession();
    accessibility_controller()->floating_menu().SetEnabled(true);
    accessibility_controller()->ShowFloatingMenuIfEnabled();
  }

  void SetOnLayoutCallback(base::RepeatingClosure closure) {
    controller()->on_layout_change_ = std::move(closure);
  }

  void SetCurrentAndAvailableImes(const std::string& current_ime_id,
                                  const std::vector<ImeInfo>& available_imes) {
    Shell::Get()->ime_controller()->RefreshIme(current_ime_id, available_imes,
                                               std::vector<ImeMenuItem>());
  }

  void ClickOnAccessibilityTrayButton() {
    views::View* button =
        GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList);
    ui::GestureEvent event = CreateTapEvent();
    button->OnGestureEvent(&event);
  }

  void EnableAndClickOnVirtualKeyboardTrayButton() {
    accessibility_controller()->virtual_keyboard().SetEnabled(true);
    GestureTapOn(
        GetMenuButton(FloatingAccessibilityView::ButtonId::kVirtualKeyboard));
  }

  // Setup one language
  void SetSingleAvailableIme() {
    ImeInfo ime_english;
    ime_english.id = ImeEnglishId;
    ime_english.name = u"English";
    ime_english.short_name = u"US";

    SetCurrentAndAvailableImes(ImeEnglishId, /*available_imes=*/{ime_english});
  }

  // Should have at least two languages to show the button
  void SetTwoAvailableImes() {
    ImeInfo ime_english;
    ime_english.id = ImeEnglishId;
    ime_english.name = u"English";
    ime_english.short_name = u"US";

    ImeInfo ime_pinyin;
    ime_pinyin.id = "ime:pinyin";
    ime_pinyin.name = u"Pinyin";
    ime_pinyin.short_name = u"拼";

    SetCurrentAndAvailableImes(ImeEnglishId,
                               /*available_imes=*/{ime_english, ime_pinyin});
  }

 protected:
  base::test::ScopedFeatureList features_;
};

TEST_F(FloatingAccessibilityControllerTest, ImeButtonNotShowWhenDisabled) {
  SetUpVisibleMenu();

  EXPECT_FALSE(IsButtonVisible(FloatingAccessibilityView::ButtonId::kIme));
}

TEST_F(FloatingAccessibilityControllerTest, ImeButtonShownWhenEnabled) {
  features_.InitAndEnableFeature(features::kKioskEnableImeButton);

  SetUpVisibleMenu();

  EXPECT_TRUE(IsButtonVisible(FloatingAccessibilityView::ButtonId::kIme));
}

TEST_F(FloatingAccessibilityControllerTest, ImeButtonHiddenWhenSingleLanguage) {
  features_.InitAndEnableFeature(features::kKioskEnableImeButton);

  SetSingleAvailableIme();
  SetUpVisibleMenu();

  EXPECT_FALSE(IsButtonVisible(FloatingAccessibilityView::ButtonId::kIme));
}

TEST_F(FloatingAccessibilityControllerTest, KioskImeTrayVisibility) {
  features_.InitAndEnableFeature(features::kKioskEnableImeButton);

  SetUpVisibleMenu();

  // Tray bubble is visible when  a user taps on the IME icon.
  GetImeTray()->PerformAction(CreateTapEvent());
  EXPECT_TRUE(IsImeTrayShown());

  // Tray bubble is invisible when the user clicks on the IME icon again.
  GetImeTray()->PerformAction(CreateTapEvent());
  EXPECT_FALSE(IsImeTrayShown());
}

TEST_F(FloatingAccessibilityControllerTest, KioskImeTrayBottomButtons) {
  features_.InitAndEnableFeature(features::kKioskEnableImeButton);

  SetUpVisibleMenu();
  EXPECT_FALSE(GetImeTray()->ShouldShowBottomButtons());
}

TEST_F(FloatingAccessibilityControllerTest, MenuIsNotShownWhenNotEnabled) {
  accessibility_controller()->ShowFloatingMenuIfEnabled();
  EXPECT_EQ(controller(), nullptr);
}

TEST_F(FloatingAccessibilityControllerTest, ShowingMenu) {
  SetUpKioskSession();
  accessibility_controller()->floating_menu().SetEnabled(true);
  accessibility_controller()->ShowFloatingMenuIfEnabled();

  EXPECT_TRUE(controller());
  EXPECT_EQ(menu_position(),
            accessibility_controller()->GetFloatingMenuPosition());
}

TEST_F(FloatingAccessibilityControllerTest, ShowingMenuAfterPrefUpdate) {
  SetUpKioskSession();

  // If we try to show the floating menu before it is enabled, nothing happens.
  accessibility_controller()->ShowFloatingMenuIfEnabled();
  EXPECT_TRUE(controller() == nullptr);

  // As soon as we enable the floating menu, it will show the floating menu
  // because we tried to show it earlier.
  accessibility_controller()->floating_menu().SetEnabled(true);
  EXPECT_FALSE(controller() == nullptr);

  // Disable the floating menu, which should cause it to be hidden.
  accessibility_controller()->floating_menu().SetEnabled(false);
  EXPECT_TRUE(controller() == nullptr);

  // Enabling it again will show the menu since we already tried to show
  // it earlier. As soon as it we request it to be shown at least once, it
  // should show/hide on enabled state change.
  accessibility_controller()->floating_menu().SetEnabled(true);
  EXPECT_FALSE(controller() == nullptr);
}

TEST_F(FloatingAccessibilityControllerTest,
       AccessibilityTrayClosedWhenVirtualKeyboardTrayIsShown) {
  SetUpVisibleMenu();

  ClickOnAccessibilityTrayButton();
  EXPECT_TRUE(detailed_view_shown());

  EnableAndClickOnVirtualKeyboardTrayButton();
  EXPECT_TRUE(GetVirtualKeyboardTray()->is_active());

  WaitUntilAccessibilityTrayClosed();

  EXPECT_FALSE(detailed_view_shown());
}

TEST_F(FloatingAccessibilityControllerTest, CanChangePosition) {
  SetUpVisibleMenu();

  accessibility_controller()->SetFloatingMenuPosition(
      FloatingMenuPosition::kTopRight);

  // Get the full root window bounds to test the position.
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // Test cases rotate clockwise.
  const struct {
    gfx::Point expected_location;
    FloatingMenuPosition expected_position;
  } kTestCases[] = {
      {gfx::Point(window_bounds.width(), window_bounds.height()),
       FloatingMenuPosition::kBottomRight},
      {gfx::Point(0, window_bounds.height()),
       FloatingMenuPosition::kBottomLeft},
      {gfx::Point(0, 0), FloatingMenuPosition::kTopLeft},
      {gfx::Point(window_bounds.width(), 0), FloatingMenuPosition::kTopRight},
  };

  // Find the autoclick menu position button.
  views::View* button =
      GetMenuButton(FloatingAccessibilityView::ButtonId::kPosition);
  ASSERT_TRUE(button) << "No position button found.";

  // Loop through all positions twice.
  for (int i = 0; i < 2; i++) {
    for (const auto& test : kTestCases) {
      SCOPED_TRACE(
          base::StringPrintf("Testing position #[%d]", test.expected_position));
      // Tap the position button.
      ui::GestureEvent event = CreateTapEvent();
      button->OnGestureEvent(&event);

      // Pref change happened.
      EXPECT_EQ(test.expected_position, menu_position());

      // Menu is in generally the correct screen location.
      EXPECT_LT(
          GetMenuViewBounds().ManhattanDistanceToPoint(test.expected_location),
          kMenuViewBoundsBuffer);
    }
  }
}

TEST_F(FloatingAccessibilityControllerTest, DetailedViewToggle) {
  SetUpVisibleMenu();

  // Find the autoclick menu position button.
  views::View* button =
      GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList);
  ASSERT_TRUE(button) << "No accessibility features list button found.";
  EXPECT_FALSE(detailed_view_shown());

  ui::GestureEvent event = CreateTapEvent();
  button->OnGestureEvent(&event);
  EXPECT_TRUE(detailed_view_shown());

  event = CreateTapEvent();
  button->OnGestureEvent(&event);
  EXPECT_FALSE(detailed_view_shown());
}

TEST_F(FloatingAccessibilityControllerTest, LocaleChangeObserver) {
  SetUpVisibleMenu();
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // RTL should position the menu on the bottom left.
  base::i18n::SetICUDefaultLocale("he");
  // Trigger the LocaleChangeObserver, which should cause a layout of the menu.
  ash::LocaleUpdateController::Get()->ConfirmLocaleChange("en", "en", "he",
                                                          base::DoNothing());
  EXPECT_TRUE(base::i18n::IsRTL());
  EXPECT_LT(
      GetMenuViewBounds().ManhattanDistanceToPoint(window_bounds.bottom_left()),
      kMenuViewBoundsBuffer);

  // LTR should position the menu on the bottom right.
  base::i18n::SetICUDefaultLocale("en");
  ash::LocaleUpdateController::Get()->ConfirmLocaleChange("he", "he", "en",
                                                          base::DoNothing());
  EXPECT_FALSE(base::i18n::IsRTL());
  EXPECT_LT(GetMenuViewBounds().ManhattanDistanceToPoint(
                window_bounds.bottom_right()),
            kMenuViewBoundsBuffer);
}

TEST_F(FloatingAccessibilityControllerTest,
       LocaleChangeObserverWithNoNotification) {
  SetUpVisibleMenu();
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // RTL should position the menu on the bottom left.
  base::i18n::SetICUDefaultLocale("he");
  // Trigger the LocaleChangeObserver, which should cause a layout of the menu.
  ash::LocaleUpdateController::Get()->OnLocaleChanged();
  EXPECT_TRUE(base::i18n::IsRTL());
  EXPECT_LT(
      GetMenuViewBounds().ManhattanDistanceToPoint(window_bounds.bottom_left()),
      kMenuViewBoundsBuffer);

  // LTR should position the menu on the bottom right.
  base::i18n::SetICUDefaultLocale("en");
  ash::LocaleUpdateController::Get()->OnLocaleChanged();
  EXPECT_FALSE(base::i18n::IsRTL());
  EXPECT_LT(GetMenuViewBounds().ManhattanDistanceToPoint(
                window_bounds.bottom_right()),
            kMenuViewBoundsBuffer);
}

// The detailed view has to be anchored to the floating menu.
TEST_F(FloatingAccessibilityControllerTest, DetailedViewPosition) {
  SetUpVisibleMenu();

  views::View* button =
      GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList);
  ui::GestureEvent event = CreateTapEvent();
  button->OnGestureEvent(&event);

  const struct { bool is_RTL; } kTestCases[] = {{true}, {false}};
  for (auto& test : kTestCases) {
    SCOPED_TRACE(base::StringPrintf("Testing rtl=#[%d]", test.is_RTL));
    // These positions should be relative to the corners of the screen
    // whether we are in RTL or LTR.
    base::i18n::SetRTLForTesting(test.is_RTL);

    // When the menu is in the top right, the detailed should be directly
    // under it and along the right side of the screen.
    controller()->SetMenuPosition(FloatingMenuPosition::kTopRight);
    EXPECT_LT(GetDetailedMenuBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().bottom_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetDetailedMenuBounds().right(), GetMenuViewBounds().right());

    // When the menu is in the bottom right, the detailed view is directly above
    // it and along the right side of the screen.
    controller()->SetMenuPosition(FloatingMenuPosition::kBottomRight);
    EXPECT_LT(GetDetailedMenuBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().top_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetDetailedMenuBounds().right(), GetMenuViewBounds().right());

    // When the menu is on the bottom left, the detailed view is directly above
    // it and along the left side of the screen.
    controller()->SetMenuPosition(FloatingMenuPosition::kBottomLeft);
    EXPECT_LT(GetDetailedMenuBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().top_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetDetailedMenuBounds().x(), GetMenuViewBounds().x());

    // When the menu is on the top left, the detailed view is directly below it
    // and along the left side of the screen.
    controller()->SetMenuPosition(FloatingMenuPosition::kTopLeft);
    EXPECT_LT(GetDetailedMenuBounds().ManhattanDistanceToPoint(
                  GetMenuViewBounds().bottom_center()),
              kMenuViewBoundsBuffer);
    EXPECT_EQ(GetDetailedMenuBounds().x(), GetMenuViewBounds().x());
  }
}

TEST_F(FloatingAccessibilityControllerTest, CollisionWithAutoclicksMenu) {
  // We expect floating accessibility menu not to move when there is autoclick
  // menu present, but to push it to avoid collision. This test is exactly the
  // same as CanChangePosition, but the autoclicks are enabled.
  SetUpVisibleMenu();
  accessibility_controller()->SetFloatingMenuPosition(
      FloatingMenuPosition::kTopRight);

  accessibility_controller()->autoclick().SetEnabled(true);

  // Get the full root window bounds to test the position.
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();

  // Test cases rotate clockwise.
  const struct {
    gfx::Point expected_location;
    FloatingMenuPosition expected_position;
  } kTestCases[] = {
      {gfx::Point(window_bounds.right(), window_bounds.bottom()),
       FloatingMenuPosition::kBottomRight},
      {gfx::Point(0, window_bounds.bottom()),
       FloatingMenuPosition::kBottomLeft},
      {gfx::Point(0, 0), FloatingMenuPosition::kTopLeft},
      {gfx::Point(window_bounds.right(), 0), FloatingMenuPosition::kTopRight},
  };

  // Find the autoclick menu position button.
  views::View* button =
      GetMenuButton(FloatingAccessibilityView::ButtonId::kPosition);
  ASSERT_TRUE(button) << "No position button found.";

  // Loop through all positions twice.
  for (int i = 0; i < 2; i++) {
    for (const auto& test : kTestCases) {
      SCOPED_TRACE(
          base::StringPrintf("Testing position #[%d]", test.expected_position));
      // Tap the position button.
      {
        ui::GestureEvent event = CreateTapEvent();
        button->OnGestureEvent(&event);
      }

      // Pref change happened.
      EXPECT_EQ(test.expected_position, menu_position());

      // Rotate around the autoclicks menu.
      for (int j = 0; j < 4; j++) {
        // The position button on autoclicks view.
        ui::GestureEvent event = CreateTapEvent();
        autoclick_menu_view()
            ->GetViewByID(
                static_cast<int>(AutoclickMenuView::ButtonId::kPosition))
            ->OnGestureEvent(&event);

        // Menu is in generally the correct screen location.
        EXPECT_LT(GetMenuViewBounds().ManhattanDistanceToPoint(
                      test.expected_location),
                  kMenuViewBoundsBuffer);
        EXPECT_FALSE(GetMenuViewBounds().Intersects(GetAutoclickMenuBounds()));
      }
    }
  }
}

TEST_F(FloatingAccessibilityControllerTest, ActiveFeaturesButtons) {
  SetUpVisibleMenu();

  struct FeatureWithButton {
    FloatingAccessibilityView::ButtonId button_id;
    A11yFeatureType feature_type;
  } kFeatureButtons[] = {{FloatingAccessibilityView::ButtonId::kDictation,
                          A11yFeatureType::kDictation},
                         {FloatingAccessibilityView::ButtonId::kSelectToSpeak,
                          A11yFeatureType::kSelectToSpeak},
                         {FloatingAccessibilityView::ButtonId::kVirtualKeyboard,
                          A11yFeatureType::kVirtualKeyboard}};

  gfx::Rect original_bounds = GetMenuViewBounds();

  for (FeatureWithButton feature : kFeatureButtons) {
    SCOPED_TRACE(
        base::StringPrintf("Testing single feature with button id=#[%d]",
                           static_cast<int>(feature.button_id)));
    views::View* feature_button = GetMenuButton(feature.button_id);
    EXPECT_TRUE(feature_button);

    // The button should not be visible when dication is not enabled.
    EXPECT_FALSE(feature_button->GetVisible());

    // Wait for relayout.
    base::RunLoop loop_enable;
    SetOnLayoutCallback(loop_enable.QuitClosure());
    accessibility_controller()
        ->GetFeature(feature.feature_type)
        .SetEnabled(true);
    loop_enable.Run();

    EXPECT_TRUE(feature_button->GetVisible());
    // Get the full root window bounds to test the position.
    gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();
    // The menu should change its size and fit on the screen.
    EXPECT_TRUE(window_bounds.Contains(GetMenuViewBounds()));

    // After disabling dictation, menu should have the same size as it had
    // before.
    base::RunLoop loop_disable;
    SetOnLayoutCallback(loop_disable.QuitClosure());
    accessibility_controller()
        ->GetFeature(feature.feature_type)
        .SetEnabled(false);
    EXPECT_EQ(GetMenuViewBounds(), original_bounds);

    SetOnLayoutCallback(base::RepeatingClosure());
  }

  {
    base::RunLoop loop_enable;
    SetOnLayoutCallback(base::BarrierClosure(std::size(kFeatureButtons),
                                             loop_enable.QuitClosure()));
    // Enable all features.
    for (FeatureWithButton feature : kFeatureButtons)
      accessibility_controller()
          ->GetFeature(feature.feature_type)
          .SetEnabled(true);
    loop_enable.Run();
  }
  gfx::Rect window_bounds = Shell::GetPrimaryRootWindow()->bounds();
  // The menu should change its size and fit on the screen.
  EXPECT_TRUE(window_bounds.Contains(GetMenuViewBounds()));
  {
    base::RunLoop loop_disable;
    SetOnLayoutCallback(base::BarrierClosure(std::size(kFeatureButtons),
                                             loop_disable.QuitClosure()));
    // Enable all features.
    // Dicable all features.
    for (FeatureWithButton feature : kFeatureButtons)
      accessibility_controller()
          ->GetFeature(feature.feature_type)
          .SetEnabled(false);
    loop_disable.Run();
  }
  EXPECT_EQ(GetMenuViewBounds(), original_bounds);
}

TEST_F(FloatingAccessibilityControllerTest, AccelatorFocusMenu) {
  SetUpVisibleMenu();

  ASSERT_TRUE(widget());
  views::FocusManager* focus_manager = widget()->GetFocusManager();

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kFocusShelf, {});
  // If nothing else is enabled, it should focus on the detailed view button.
  EXPECT_EQ(focus_manager->GetFocusedView(),
            GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList));

  // Focus should be reset if advanced through the menu.
  focus_manager->AdvanceFocus(false /* reverse */);
  EXPECT_NE(focus_manager->GetFocusedView(),
            GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList));

  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kFocusShelf, {});
  // It should get back to the settings list button.
  EXPECT_EQ(focus_manager->GetFocusedView(),
            GetMenuButton(FloatingAccessibilityView::ButtonId::kSettingsList));

  // Now, enable virtual keyboard and spoken feedback.
  accessibility_controller()->virtual_keyboard().SetEnabled(true);
  accessibility_controller()->select_to_speak().SetEnabled(true);

  // We should be focused on the first button in the menu.
  // Order: select to speak, virtual keyboard, settings menu, position.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kFocusShelf, {});
  EXPECT_EQ(focus_manager->GetFocusedView(),
            GetMenuButton(FloatingAccessibilityView::ButtonId::kSelectToSpeak));
}

TEST_F(FloatingAccessibilityControllerTest, ShowingAlreadyEnabledFeatures) {
  accessibility_controller()->select_to_speak().SetEnabled(true);
  accessibility_controller()->dictation().SetEnabled(true);
  accessibility_controller()->virtual_keyboard().SetEnabled(true);
  SetUpVisibleMenu();

  EXPECT_TRUE(GetMenuButton(FloatingAccessibilityView::ButtonId::kSelectToSpeak)
                  ->GetVisible());
  EXPECT_TRUE(GetMenuButton(FloatingAccessibilityView::ButtonId::kDictation)
                  ->GetVisible());
  EXPECT_TRUE(
      GetMenuButton(FloatingAccessibilityView::ButtonId::kVirtualKeyboard)
          ->GetVisible());
}

}  // namespace ash
