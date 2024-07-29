// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/folder_header_view.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_test_model.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_folder_view.h"
#include "ash/app_list/views/apps_grid_view_test_api.h"
#include "ash/app_list/views/scrollable_apps_grid_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/views/controls/textfield/textfield.h"

namespace ash {

// Parameterized by whether kJelly feature is enabled.
class FolderHeaderViewTest : public AshTestBase {
 public:
  FolderHeaderViewTest() = default;
  FolderHeaderViewTest(const FolderHeaderViewTest&) = delete;
  FolderHeaderViewTest& operator=(const FolderHeaderViewTest&) = delete;

  ~FolderHeaderViewTest() override = default;

  // testing::Test overrides:
  void SetUp() override {
    AshTestBase::SetUp();
    model_ = GetAppListTestHelper()->model();
    // `folder_header_view_` is set when the folder is opened. This allows test
    // cases to configure the model before opening the folder.
  }

  // Assumes the folder is the first item in the grid.
  void ShowAppListAndOpenFolder() {
    auto* helper = GetAppListTestHelper();
    helper->ShowAppList();
    AppsGridView* apps_grid_view = helper->GetScrollableAppsGridView();
    ASSERT_TRUE(apps_grid_view);
    test::AppsGridViewTestApi(apps_grid_view).PressItemAt(0);
    ASSERT_TRUE(helper->IsInFolderView());

    folder_header_view_ = helper->GetBubbleFolderView()->folder_header_view();
  }

 protected:
  void UpdateFolderName(const std::string& name) {
    std::u16string folder_name = base::UTF8ToUTF16(name);
    views::Textfield* textfield =
        folder_header_view_->GetFolderNameViewForTest();
    textfield->SetText(u"");
    textfield->InsertText(
        folder_name,
        ui::TextInputClient::InsertTextCursorBehavior::kMoveCursorAfterText);
  }

  const std::string GetFolderNameFromUI() {
    return base::UTF16ToUTF8(folder_header_view_->GetFolderNameForTest());
  }

  bool CanEditFolderName() {
    return folder_header_view_->IsFolderNameEnabledForTest();
  }

  void FocusText() { folder_header_view_->SetTextFocus(); }

  bool HasTextFocus() { return folder_header_view_->HasTextFocus(); }

  void SendKey(ui::KeyboardCode key_code, int flags = ui::EF_NONE) {
    PressAndReleaseKey(key_code, flags);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<test::AppListTestModel, DanglingUntriaged> model_ = nullptr;
  raw_ptr<FolderHeaderView, DanglingUntriaged> folder_header_view_ = nullptr;
};

TEST_F(FolderHeaderViewTest, WhitespaceCollapsedWhenFolderNameViewLosesFocus) {
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  ShowAppListAndOpenFolder();
  views::View* name_view = folder_header_view_->GetFolderNameViewForTest();

  name_view->RequestFocus();
  UpdateFolderName("  N     A  ");
  name_view->GetFocusManager()->ClearFocus();

  // Expect that the folder name contains the same string with collapsed
  // whitespace.
  EXPECT_EQ("N A", folder_item->name());
}

TEST_F(FolderHeaderViewTest, MaxFolderNameLength) {
  // Creating a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  ShowAppListAndOpenFolder();
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_TRUE(CanEditFolderName());

  // Update UI to set folder name to really long one beyond its maximum limit
  // If folder name is set beyond the maximum char limit, it should revert to
  // the previous valid folder name.
  std::string max_len_name;
  for (int i = 0; i < folder_header_view_->GetMaxFolderNameCharLengthForTest();
       ++i) {
    max_len_name += "a";
  }
  std::string too_long_name = max_len_name + "a";

  // Expect that the folder name does not change, and does not truncate
  UpdateFolderName(too_long_name);
  EXPECT_EQ(std::string(), folder_item->name());

  // Expect the folder does change to the new valid name given
  UpdateFolderName(max_len_name);
  EXPECT_EQ(max_len_name, folder_item->name());

  // Expect that the name is reverted to the previous valid name and is not
  // truncated
  too_long_name.insert(5, "testing");
  UpdateFolderName(too_long_name);
  EXPECT_EQ(max_len_name, folder_item->name());
}

TEST_F(FolderHeaderViewTest, OemFolderNameNotEditable) {
  model_->CreateAndAddOemFolder();
  ShowAppListAndOpenFolder();
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_FALSE(CanEditFolderName());
}

namespace {

// Sends a tap gesture with events corresponding to touch-down and touch-up.
// This is a template to support a |handler| with an OnGestureEvent() method
// such as views::Widget or views::View.
template <typename GestureHandler>
void SendTap(GestureHandler* handler, const gfx::Point& location) {
  ui::GestureEvent tap_down(
      location.x(), location.y(), 0, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));
  handler->OnGestureEvent(&tap_down);
  ui::GestureEvent tap_up(location.x(), location.y(), 0, base::TimeTicks::Now(),
                          ui::GestureEventDetails(ui::EventType::kGestureTap));
  handler->OnGestureEvent(&tap_up);
}

template <typename EventHandler>
void SendPress(EventHandler* handler, const gfx::Point& location) {
  ui::MouseEvent press_down(ui::EventType::kMousePressed,
                            gfx::PointF(location.x(), location.y()),
                            gfx::PointF(0, 0), base::TimeTicks::Now(), 0, 0);
  handler->OnMouseEvent(&press_down);
  ui::MouseEvent press_up(ui::EventType::kMouseReleased,
                          gfx::PointF(location.x(), location.y()),
                          gfx::PointF(0, 0), base::TimeTicks::Now(), 0, 0);
  handler->OnMouseEvent(&press_up);
}

}  // namespace

// Tests that when folder name is small, the folder name textfield is triggered
// by only tap when on the textfieldd or near it to the left/right.
TEST_F(FolderHeaderViewTest, TriggerFolderRenameAfterTappingNearFolderName) {
  // Create a folder with a small name.
  model_->CreateAndPopulateFolderWithApps(2);
  ShowAppListAndOpenFolder();
  UpdateFolderName("ab");

  // Get in screen bounds of folder name
  views::View* name_view = folder_header_view_->GetFolderNameViewForTest();
  const gfx::Rect name_view_bounds = name_view->GetBoundsInScreen();

  // Tap folder name and check that folder renaming is triggered.
  SendTap(name_view, name_view_bounds.CenterPoint());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(name_view->HasFocus());

  // Clear focus from the folder name.
  name_view->GetFocusManager()->ClearFocus();
  ASSERT_FALSE(name_view->HasFocus());

  // Test that tapping near (but not directly on) the folder name still
  // triggers folder rename.
  gfx::Point right_of_name_view = name_view_bounds.right_center();
  right_of_name_view.Offset(2, 0);
  SendTap(name_view, right_of_name_view);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(name_view->HasFocus());

  // Clear focus from the folder name.
  name_view->GetFocusManager()->ClearFocus();
  ASSERT_FALSE(name_view->HasFocus());

  // Test that clicking in the same spot won't trigger folder rename.
  SendPress(name_view, right_of_name_view);
  EXPECT_FALSE(name_view->HasFocus());
}

// Test that hitting the return key sets the folder name.
TEST_F(FolderHeaderViewTest, SetFolderNameOnReturn) {
  // Create a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  ShowAppListAndOpenFolder();
  ASSERT_EQ("", GetFolderNameFromUI());
  ASSERT_TRUE(CanEditFolderName());

  // Focus the text.
  FocusText();
  ASSERT_TRUE(HasTextFocus());

  // Set the folder name.
  UpdateFolderName("ret");
  EXPECT_EQ("ret", GetFolderNameFromUI());

  // Press return.
  SendKey(ui::VKEY_RETURN);

  // Make sure the return press unfocused the text and registered the name
  // change.
  EXPECT_EQ(false, HasTextFocus());
  EXPECT_EQ("ret", folder_item->name());
}

// Test that hitting the escape key reverts the folder name.
TEST_F(FolderHeaderViewTest, RevertFolderNameOnEscape) {
  // Create a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  ShowAppListAndOpenFolder();
  ASSERT_EQ("", GetFolderNameFromUI());
  ASSERT_TRUE(CanEditFolderName());

  // Focus the text.
  FocusText();
  ASSERT_TRUE(HasTextFocus());

  // Set the folder name.
  UpdateFolderName("esc");
  EXPECT_EQ("esc", GetFolderNameFromUI());

  // Press escape.a
  SendKey(ui::VKEY_ESCAPE);

  // Make sure the escape press unfocused the text and reverted the name change.
  EXPECT_EQ(false, HasTextFocus());
  EXPECT_EQ("", folder_item->name());
}

}  // namespace ash
