// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/folder_header_view.h"

#include <stddef.h>

#include <memory>
#include <string>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/test/app_list_test_model.h"
#include "ash/app_list/views/folder_header_view_delegate.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/views_test_base.h"

namespace ash {
namespace test {

namespace {

class TestFolderHeaderViewDelegate : public FolderHeaderViewDelegate {
 public:
  TestFolderHeaderViewDelegate() {}
  ~TestFolderHeaderViewDelegate() override {}

  // FolderHeaderViewDelegate
  void NavigateBack(AppListFolderItem* item,
                    const ui::Event& event_flags) override {}

  void GiveBackFocusToSearchBox() override {}

  void SetItemName(AppListFolderItem* item, const std::string& name) override {
    folder_name_ = name;
  }

  const std::string& folder_name() const { return folder_name_; }

 private:
  std::string folder_name_;

  DISALLOW_COPY_AND_ASSIGN(TestFolderHeaderViewDelegate);
};

}  // namespace

class FolderHeaderViewTest : public views::ViewsTestBase {
 public:
  FolderHeaderViewTest() {}
  ~FolderHeaderViewTest() override {}

  // testing::Test overrides:
  void SetUp() override {
    views::ViewsTestBase::SetUp();
    model_ = std::make_unique<AppListTestModel>();
    delegate_ = std::make_unique<TestFolderHeaderViewDelegate>();

    // Create a widget so that the FolderNameView can be focused.
    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams params = views::ViewsTestBase::CreateParams(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    textfield_ = std::make_unique<views::Textfield>();
    folder_header_view_ = widget_->SetContentsView(
        std::make_unique<FolderHeaderView>(delegate_.get()));
  }

  void TearDown() override {
    widget_->Close();
    widget_.reset();
    textfield_.reset();
    delegate_.reset();
    views::ViewsTestBase::TearDown();
  }

 protected:
  void UpdateFolderName(const std::string& name) {
    base::string16 folder_name = base::UTF8ToUTF16(name);
    folder_header_view_->SetFolderNameForTest(folder_name);
    folder_header_view_->ContentsChanged(textfield_.get(), folder_name);
  }

  const std::string GetFolderNameFromUI() {
    return base::UTF16ToUTF8(folder_header_view_->GetFolderNameForTest());
  }

  bool CanEditFolderName() {
    return folder_header_view_->IsFolderNameEnabledForTest();
  }

  void UpdatePreviousCursorPosition(const size_t previous_cursor_position) {
    folder_header_view_->SetPreviousCursorPositionForTest(
        previous_cursor_position);
  }

  void UpdatePreviousFolderName(const base::string16& previous_name) {
    folder_header_view_->SetPreviousFolderNameForTest(previous_name);
  }

  std::unique_ptr<AppListTestModel> model_;
  FolderHeaderView* folder_header_view_ = nullptr;  // owned by |widget_|.
  std::unique_ptr<TestFolderHeaderViewDelegate> delegate_;
  std::unique_ptr<views::Textfield> textfield_;
  std::unique_ptr<views::Widget> widget_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FolderHeaderViewTest);
};

TEST_F(FolderHeaderViewTest, SetFolderName) {
  // Creating a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  folder_header_view_->SetFolderItem(folder_item);
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_TRUE(CanEditFolderName());

  // Update UI to set folder name to "test folder".
  UpdateFolderName("test folder");
  EXPECT_EQ("test folder", delegate_->folder_name());
}

TEST_F(FolderHeaderViewTest, WhitespaceCollapsedWhenFolderNameViewLosesFocus) {
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  folder_header_view_->SetFolderItem(folder_item);
  views::View* name_view = folder_header_view_->GetFolderNameViewForTest();

  name_view->RequestFocus();
  UpdateFolderName("  N     A  ");
  widget_->GetFocusManager()->ClearFocus();

  // Expect that the folder name contains the same string with collapsed
  // whitespace.
  EXPECT_EQ("N A", delegate_->folder_name());
}

TEST_F(FolderHeaderViewTest, MaxFolderNameLength) {
  // Creating a folder with empty folder name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  folder_header_view_->SetFolderItem(folder_item);
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_TRUE(CanEditFolderName());

  // Update UI to set folder name to really long one beyond its maximum limit
  // If folder name is set beyond the maximum char limit, it should revert to
  // the previous valid folder name.
  std::string max_len_name;
  for (size_t i = 0; i < AppListConfig::instance().max_folder_name_chars();
       ++i) {
    max_len_name += "a";
  }
  std::string too_long_name = max_len_name + "a";
  UpdatePreviousCursorPosition(0);
  UpdatePreviousFolderName(base::string16());

  // Expect that the folder name does not change, and does not truncate
  UpdateFolderName(too_long_name);
  EXPECT_EQ(std::string(), delegate_->folder_name());

  // Expect the folder does change to the new valid name given
  UpdateFolderName(max_len_name);
  EXPECT_EQ(max_len_name, delegate_->folder_name());

  // Expect that the name is reverted to the previous valid name and is not
  // truncated
  too_long_name.insert(5, "testing");
  UpdateFolderName(too_long_name);
  EXPECT_EQ(max_len_name, delegate_->folder_name());
}

TEST_F(FolderHeaderViewTest, OemFolderNameNotEditable) {
  AppListFolderItem* folder_item = model_->CreateAndAddOemFolder();
  folder_header_view_->SetFolderItem(folder_item);
  EXPECT_EQ("", GetFolderNameFromUI());
  EXPECT_FALSE(CanEditFolderName());
}

namespace {

// Sends a tap gesture with events corresponding to touch-down and touch-up.
// This is a template to support a |handler| with an OnGestureEvent() method
// that isn't a ui::EventHandler implementation.
template <typename GestureHandler>
void SendTap(GestureHandler* handler, const gfx::Point& location) {
  ui::GestureEvent tap_down(
      location.x(), location.y(), 0, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::ET_GESTURE_TAP_DOWN));
  handler->OnGestureEvent(&tap_down);
  ui::GestureEvent tap_up(
      location.x(), location.y(), 0, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::ET_GESTURE_TAP));
  handler->OnGestureEvent(&tap_up);
}

template <typename EventHandler>
void SendPress(EventHandler* handler, const gfx::Point& location) {
  ui::MouseEvent press_down(ui::ET_MOUSE_PRESSED,
                            gfx::PointF(location.x(), location.y()),
                            gfx::PointF(0, 0), base::TimeTicks::Now(), 0, 0);
  handler->OnMouseEvent(&press_down);
  ui::MouseEvent press_up(ui::ET_MOUSE_RELEASED,
                          gfx::PointF(location.x(), location.y()),
                          gfx::PointF(0, 0), base::TimeTicks::Now(), 0, 0);
  handler->OnMouseEvent(&press_up);
}

}  // namespace

// Tests that when folder name is small, the folder name textfield is triggered
// by only tap when on the textfieldd or near it to the left/right.
TEST_F(FolderHeaderViewTest, TriggerFolderRenameAfterTappingNearFolderName) {
  // Create a folder with a small name.
  AppListFolderItem* folder_item = model_->CreateAndPopulateFolderWithApps(2);
  folder_header_view_->SetFolderItem(folder_item);
  UpdateFolderName("ab");

  // Get in screen bounds of folder name
  views::View* name_view = folder_header_view_->GetFolderNameViewForTest();
  const gfx::Rect name_view_bounds = name_view->GetBoundsInScreen();

  // Tap folder name and check that folder renaming is triggered.
  SendTap(name_view, name_view_bounds.CenterPoint());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(name_view->HasFocus());

  // Clear focus from the folder name.
  widget_->GetFocusManager()->ClearFocus();
  ASSERT_FALSE(name_view->HasFocus());

  // Test that tapping near (but not directly on) the folder name still
  // triggers folder rename.
  // Tap folder name and check that folder renaming is triggered.
  gfx::Point right_of_name_view = name_view_bounds.right_center();
  right_of_name_view.Offset(2, 0);
  SendTap(widget_.get(), right_of_name_view);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(name_view->HasFocus());

  // Clear focus from the folder name.
  widget_->GetFocusManager()->ClearFocus();
  ASSERT_FALSE(name_view->HasFocus());

  // Test that clicking in the same spot won't trigger folder rename.
  SendPress(widget_.get(), right_of_name_view);
  EXPECT_FALSE(name_view->HasFocus());
}

}  // namespace test
}  // namespace ash
