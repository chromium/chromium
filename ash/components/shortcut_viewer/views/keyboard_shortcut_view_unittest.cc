// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/views/keyboard_shortcut_view.h"

#include <set>

#include "ash/components/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/components/shortcut_viewer/views/keyboard_shortcut_item_view.h"
#include "ash/components/shortcut_viewer/views/ksv_search_box_view.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/window.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

class KeyboardShortcutViewTest : public ash::AshTestBase {
 public:
  KeyboardShortcutViewTest() = default;
  ~KeyboardShortcutViewTest() override = default;

  views::Widget* Toggle() {
    return KeyboardShortcutView::Toggle(CurrentContext());
  }

  // ash::AshTestBase:
  void SetUp() override {
    ash::AshTestBase::SetUp();
    // Simulate the complete listing of input devices, required by the viewer.
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();
  }

 protected:
  size_t GetTabCount() const {
    DCHECK(GetView());
    return GetView()->GetTabCountForTesting();
  }

  const std::vector<std::unique_ptr<KeyboardShortcutItemView>>&
  GetShortcutViews() {
    DCHECK(GetView());
    return GetView()->GetShortcutViewsForTesting();
  }

  KSVSearchBoxView* GetSearchBoxView() {
    DCHECK(GetView());
    return GetView()->GetSearchBoxViewForTesting();
  }

  void KeyPress(ui::KeyboardCode key_code, bool should_insert) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    GetSearchBoxView()->OnKeyEvent(&event);
    if (!should_insert)
      return;

    // Emulates the input method.
    if (::isalnum(static_cast<int>(key_code))) {
      base::char16 character = ::tolower(static_cast<int>(key_code));
      GetSearchBoxView()->search_box()->InsertText(
          base::string16(1, character));
    }
  }

  base::HistogramTester histograms_;

 private:
  KeyboardShortcutView* GetView() const {
    return KeyboardShortcutView::GetInstanceForTesting();
  }

  DISALLOW_COPY_AND_ASSIGN(KeyboardShortcutViewTest);
};

// Shows and closes the widget for KeyboardShortcutViewer.
TEST_F(KeyboardShortcutViewTest, ShowAndClose) {
  // Show the widget.
  views::Widget* widget = Toggle();
  EXPECT_TRUE(widget);

  // Cleaning up.
  widget->CloseNow();
}

TEST_F(KeyboardShortcutViewTest, StartupTimeHistogram) {
  views::Widget* widget = Toggle();
  ui::WaitForNextFrameToBePresented(widget->GetCompositor());
  histograms_.ExpectTotalCount("Keyboard.ShortcutViewer.StartupTime", 1);
  widget->CloseNow();
}

// KeyboardShortcutViewer window should be centered in screen.
TEST_F(KeyboardShortcutViewTest, CenterWindowInScreen) {
  // Show the widget.
  views::Widget* widget = Toggle();
  EXPECT_TRUE(widget);

  gfx::Rect root_window_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(widget->GetNativeWindow()->GetRootWindow())
          .work_area();
  gfx::Rect shortcuts_window_bounds =
      widget->GetNativeWindow()->GetBoundsInScreen();
  EXPECT_EQ(root_window_bounds.CenterPoint().x(),
            shortcuts_window_bounds.CenterPoint().x());
  EXPECT_EQ(root_window_bounds.CenterPoint().y(),
            shortcuts_window_bounds.CenterPoint().y());

  // Cleaning up.
  widget->CloseNow();
}

// Test that the number of side tabs equals to the number of categories.
TEST_F(KeyboardShortcutViewTest, SideTabsCount) {
  // Show the widget.
  views::Widget* widget = Toggle();

  size_t category_number = 0;
  ShortcutCategory current_category = ShortcutCategory::kUnknown;
  for (const auto& item_view : GetShortcutViews()) {
    const ShortcutCategory category = item_view->category();
    if (current_category != category) {
      DCHECK(current_category < category);
      ++category_number;
      current_category = category;
    }
  }
  EXPECT_EQ(GetTabCount(), category_number);

  // Cleaning up.
  widget->CloseNow();
}

// Test that the top line in two views should be center aligned.
TEST_F(KeyboardShortcutViewTest, TopLineCenterAlignedInItemView) {
  // Show the widget.
  views::Widget* widget = Toggle();

  for (const auto& item_view : GetShortcutViews()) {
    // We only initialize the first visible category and other non-visible panes
    // are deferred initialized.
    if (item_view->category() != ShortcutCategory::kPopular)
      continue;

    ASSERT_EQ(2u, item_view->children().size());

    // The top lines in both |description_label_view_| and
    // |shortcut_label_view_| should be center aligned. Only need to check one
    // view in the top line, because StyledLabel always center align all the
    // views in a line.
    const views::View* description = item_view->children()[0];
    const views::View* shortcut = item_view->children()[1];
    EXPECT_EQ(
        description->children().front()->GetBoundsInScreen().CenterPoint().y(),
        shortcut->children().front()->GetBoundsInScreen().CenterPoint().y());
  }

  // Cleaning up.
  widget->CloseNow();
}

// Test that the focus is on search box when window inits and exits search mode.
TEST_F(KeyboardShortcutViewTest, FocusOnSearchBox) {
  // Show the widget.
  views::Widget* widget = Toggle();

  // Case 1: when window creates. The focus should be on search box.
  EXPECT_TRUE(GetSearchBoxView()->search_box()->HasFocus());

  // Press a key should enter search mode.
  KeyPress(ui::VKEY_A, /*should_insert=*/true);
  EXPECT_TRUE(GetSearchBoxView()->back_button()->GetVisible());
  EXPECT_FALSE(GetSearchBoxView()->search_box()->GetText().empty());

  // Case 2: Exit search mode by clicking |back_button|. The focus should be on
  // search box.
  GetSearchBoxView()->ButtonPressed(
      GetSearchBoxView()->back_button(),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                     base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON,
                     ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(GetSearchBoxView()->search_box()->GetText().empty());
  EXPECT_TRUE(GetSearchBoxView()->search_box()->HasFocus());

  // Enter search mode again.
  KeyPress(ui::VKEY_A, /*should_insert=*/true);
  EXPECT_FALSE(GetSearchBoxView()->search_box()->GetText().empty());

  // Case 3: Exit search mode by pressing |VKEY_ESCAPE|. The focus should be on
  // search box.
  KeyPress(ui::VKEY_ESCAPE, /*should_insert=*/false);
  EXPECT_TRUE(GetSearchBoxView()->search_box()->GetText().empty());
  EXPECT_TRUE(GetSearchBoxView()->search_box()->HasFocus());

  // Cleaning up.
  widget->CloseNow();
}

// Test that the window can be closed by accelerator.
TEST_F(KeyboardShortcutViewTest, CloseWindowByAccelerator) {
  // Show the widget.
  views::Widget* widget = Toggle();
  EXPECT_FALSE(widget->IsClosed());

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsClosed());
}

// Test that the window can be activated or closed by toggling.
TEST_F(KeyboardShortcutViewTest, ToggleWindow) {
  // Show the widget.
  views::Widget* widget = Toggle();
  EXPECT_FALSE(widget->IsClosed());

  // Call |Toggle()| to activate the inactive widget.
  EXPECT_TRUE(widget->IsActive());
  widget->Deactivate();
  EXPECT_FALSE(widget->IsActive());
  Toggle();
  EXPECT_TRUE(widget->IsActive());

  // Call |Toggle()| to close the active widget.
  Toggle();
  EXPECT_TRUE(widget->IsClosed());
}

}  // namespace keyboard_shortcut_viewer
