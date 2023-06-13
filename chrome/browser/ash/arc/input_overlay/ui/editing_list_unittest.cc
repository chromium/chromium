// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/arc/input_overlay/test/view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"

namespace arc::input_overlay {

class EditingListTest : public ViewTestBase {
 public:
  EditingListTest() = default;
  ~EditingListTest() override = default;

  size_t GetActionListItemsSize() {
    DCHECK(editing_list_->scroll_content_);
    DCHECK(editing_list_);
    if (editing_list_->HasControls()) {
      return editing_list_->scroll_content_->children().size();
    }
    return 0;
  }

  size_t GetActionViewSize() {
    DCHECK(input_mapping_view_);
    return input_mapping_view_->children().size();
  }

  size_t GetTouchInjectorActionSize() {
    DCHECK(touch_injector_);
    return touch_injector_->actions().size();
  }

  void PressAddButton() {
    DCHECK(editing_list_);
    editing_list_->OnAddButtonPressed();
  }

  std::unique_ptr<EditingList> editing_list_;

 private:
  void SetUp() override {
    ViewTestBase::SetUp();
    InitWithFeature(ash::features::kArcInputOverlayBeta);
    SetDisplayMode(DisplayMode::kEdit);

    editing_list_ =
        std::make_unique<EditingList>(display_overlay_controller_.get());
    editing_list_->Init();
    DCHECK(editing_list_->scroll_content_);
  }

  void TearDown() override {
    editing_list_.reset();
    ViewTestBase::TearDown();
  }
};

TEST_F(EditingListTest, TestEditingListAddNewAction) {
  EXPECT_EQ(2u, GetActionListItemsSize());
  EXPECT_EQ(2u, GetActionViewSize());
  EXPECT_EQ(2u, GetTouchInjectorActionSize());
  PressAddButton();
  EXPECT_EQ(3u, GetActionListItemsSize());
  EXPECT_EQ(3u, GetActionViewSize());
  EXPECT_EQ(3u, GetTouchInjectorActionSize());
}

}  // namespace arc::input_overlay
