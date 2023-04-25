// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/views/keyboard_shortcut_view.h"

#include <set>

#include "ash/shortcut_viewer/keyboard_shortcut_viewer_metadata.h"
#include "ash/shortcut_viewer/views/keyboard_shortcut_item_view.h"
#include "ash/shortcut_viewer/views/ksv_search_box_view.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "chromeos/ui/base/window_properties.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

namespace {

constexpr SkColor kTitleAndFrameColorLight = SK_ColorWHITE;
constexpr SkColor kTitleAndFrameColorDark = gfx::kGoogleGrey900;

}  // namespace

class KeyboardShortcutViewTest : public ash::AshTestBase {
 public:
  KeyboardShortcutViewTest()
      : ash::AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  KeyboardShortcutViewTest(const KeyboardShortcutViewTest&) = delete;
  KeyboardShortcutViewTest& operator=(const KeyboardShortcutViewTest&) = delete;

  ~KeyboardShortcutViewTest() override = default;

  views::Widget* Toggle() { return KeyboardShortcutView::Toggle(GetContext()); }

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
  GetShortcutViews() const {
    DCHECK(GetView());
    return GetView()->GetShortcutViewsForTesting();
  }

  KSVSearchBoxView* GetSearchBoxView() {
    DCHECK(GetView());
    return GetView()->GetSearchBoxViewForTesting();
  }

  const std::vector<KeyboardShortcutItemView*>& GetFoundShortcutItems() const {
    DCHECK(GetView());
    return GetView()->GetFoundShortcutItemsForTesting();
  }

  void KeyPress(ui::KeyboardCode key_code, bool should_insert) {
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, ui::EF_NONE);
    GetSearchBoxView()->OnKeyEvent(&event);
    if (!should_insert)
      return;

    // Emulates the input method.
    if (::isalnum(static_cast<int>(key_code))) {
      char16_t character = ::tolower(static_cast<int>(key_code));
      GetSearchBoxView()->search_box()->InsertText(
          std::u16string(1, character),
          ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
    }
  }

  KeyboardShortcutView* GetView() const {
    return KeyboardShortcutView::GetInstanceForTesting();
  }
};

// Shows and closes the widget for KeyboardShortcutViewer.
TEST_F(KeyboardShortcutViewTest, ShowAndClose) {
  // Show the widget.
  views::Widget* widget = Toggle();
  EXPECT_TRUE(widget);

  // Cleaning up.
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
  ash::ShortcutCategory current_category = ash::ShortcutCategory::kUnknown;
  for (const auto& item_view : GetShortcutViews()) {
    const ash::ShortcutCategory category = item_view->category();
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
    if (item_view->category() != ash::ShortcutCategory::kPopular)
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
  EXPECT_TRUE(GetSearchBoxView()->close_button()->GetVisible());
  EXPECT_FALSE(GetSearchBoxView()->search_box()->GetText().empty());

  // Case 2: Exit search mode by clicking |close_button|. The focus should be on
  // search box.
  views::test::ButtonTestApi(GetSearchBoxView()->close_button())
      .NotifyClick(ui::MouseEvent(
          ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(), base::TimeTicks(),
          ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
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

// Test that the window can be closed by accelerator (CTRL + SHIFT + W).
TEST_F(KeyboardShortcutViewTest, CloseWindowByAcceleratorCtrlShiftW) {
  // Show the widget.
  views::Widget* widget = Toggle();
  EXPECT_FALSE(widget->IsClosed());

  ui::test::EventGenerator* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_W,
                            ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
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

// Test that the sub-labels of the |description_label_view| in the search
// results page have the same height and are vertically aligned.
TEST_F(KeyboardShortcutViewTest, ShouldAlignSubLabelsInSearchResults) {
  // Show the widget.
  Toggle();

  EXPECT_TRUE(GetFoundShortcutItems().empty());
  // Type a letter and show the search results.
  KeyPress(ui::VKEY_A, /*should_insert=*/true);
  auto time_out = base::Milliseconds(300);
  task_environment()->FastForwardBy(time_out);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetFoundShortcutItems().empty());

  for (const auto* item_view : GetFoundShortcutItems()) {
    ASSERT_EQ(2u, item_view->children().size());

    const views::View* description = item_view->children()[0];
    // Skip if it only has one label.
    if (description->children().size() == 1)
      continue;

    // The sub-labels in |description_label_view_| have the same height and are
    // vertically aligned in each line.
    int height = 0;
    int center_y = 0;
    for (const auto* child : description->children()) {
      // The first view in each line.
      if (child->bounds().x() == 0) {
        height = child->bounds().height();
        center_y = child->bounds().CenterPoint().y();
        continue;
      }

      EXPECT_EQ(height, child->GetPreferredSize().height());
      EXPECT_EQ(center_y, child->bounds().CenterPoint().y());
    }
  }
}

TEST_F(KeyboardShortcutViewTest, FrameAndBackgroundColorUpdates) {
  ash::AshTestBase::SimulateGuestLogin();
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->SetDarkModeEnabledForTest(false);
  // Show the widget.
  Toggle();

  auto* window = GetSearchBoxView()->GetWidget()->GetNativeWindow();
  EXPECT_EQ(kTitleAndFrameColorLight,
            window->GetProperty(chromeos::kFrameActiveColorKey));
  EXPECT_EQ(kTitleAndFrameColorLight,
            window->GetProperty(chromeos::kFrameInactiveColorKey));
  EXPECT_EQ(kTitleAndFrameColorLight, GetView()->GetBackground()->get_color());

  dark_light_mode_controller->ToggleColorMode();

  EXPECT_EQ(kTitleAndFrameColorDark,
            window->GetProperty(chromeos::kFrameActiveColorKey));
  EXPECT_EQ(kTitleAndFrameColorDark,
            window->GetProperty(chromeos::kFrameInactiveColorKey));
  EXPECT_EQ(kTitleAndFrameColorDark, GetView()->GetBackground()->get_color());
}

// TODO(https://crbug.com/1439747): Flaky on ASAN, probably due to hard-coded
// timeout.
#if BUILDFLAG(IS_LINUX) && defined(ADDRESS_SANITIZER)
#define MAYBE_AccessibilityProperties DISABLED_AccessibilityProperties
#else
#define MAYBE_AccessibilityProperties AccessibilityProperties
#endif
TEST_F(KeyboardShortcutViewTest, MAYBE_AccessibilityProperties) {
  // Show the widget.
  Toggle();

  EXPECT_TRUE(GetFoundShortcutItems().empty());
  // Type a letter and show the search results.
  KeyPress(ui::VKEY_A, /*should_insert=*/true);
  auto time_out = base::Milliseconds(300);
  task_environment()->FastForwardBy(time_out);
  base::RunLoop().RunUntilIdle();

  const std::vector<KeyboardShortcutItemView*>& items = GetFoundShortcutItems();
  EXPECT_FALSE(items.empty());

  auto* first_item = items.front();
  ui::AXNodeData first_item_data;
  first_item->GetViewAccessibility().GetAccessibleNodeData(&first_item_data);
  EXPECT_EQ(first_item_data.role, ax::mojom::Role::kListItem);
  EXPECT_EQ(first_item->GetAccessibleRole(), first_item_data.role);
  EXPECT_FALSE(first_item->GetAccessibleName().empty());
  EXPECT_EQ(
      first_item_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      first_item->GetAccessibleName());
  EXPECT_EQ(first_item_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            1);
  EXPECT_EQ(first_item_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            static_cast<int>(items.size()));

  auto* last_item = items.back();
  ui::AXNodeData last_item_data;
  last_item->GetViewAccessibility().GetAccessibleNodeData(&last_item_data);
  EXPECT_EQ(last_item_data.role, ax::mojom::Role::kListItem);
  EXPECT_EQ(last_item->GetAccessibleRole(), last_item_data.role);
  EXPECT_EQ(
      last_item_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      last_item->GetAccessibleName());
  EXPECT_FALSE(last_item->GetAccessibleName().empty());
  EXPECT_EQ(last_item_data.GetIntAttribute(ax::mojom::IntAttribute::kPosInSet),
            static_cast<int>(items.size()));
  EXPECT_EQ(last_item_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize),
            static_cast<int>(items.size()));

  auto* list = last_item->parent();
  while (list && list->GetAccessibleRole() != ax::mojom::Role::kList) {
    list = list->parent();
  }

  EXPECT_TRUE(list);
  ui::AXNodeData list_data;
  list->GetViewAccessibility().GetAccessibleNodeData(&list_data);
  EXPECT_EQ(list_data.role, ax::mojom::Role::kList);

  auto* scroll_view = list->parent();
  while (scroll_view &&
         scroll_view->GetAccessibleRole() != ax::mojom::Role::kScrollView) {
    scroll_view = scroll_view->parent();
  }

  EXPECT_TRUE(scroll_view);
  ui::AXNodeData scroll_view_data;
  scroll_view->GetViewAccessibility().GetAccessibleNodeData(&scroll_view_data);
  EXPECT_EQ(scroll_view_data.role, ax::mojom::Role::kScrollView);
  EXPECT_FALSE(scroll_view->GetAccessibleName().empty());
  EXPECT_EQ(
      scroll_view_data.GetString16Attribute(ax::mojom::StringAttribute::kName),
      scroll_view->GetAccessibleName());
}

}  // namespace keyboard_shortcut_viewer
