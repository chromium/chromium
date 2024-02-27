// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_view.h"

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

TEST(PickerItemViewTest, DefaultIsNormalState) {
  PickerItemView item_view(base::DoNothing());

  EXPECT_EQ(item_view.GetItemState(), PickerItemView::ItemState::kNormal);
}

TEST(PickerItemViewTest, NoBackgroundInNormalState) {
  PickerItemView item_view(base::DoNothing());

  item_view.SetItemState(PickerItemView::ItemState::kNormal);

  EXPECT_FALSE(item_view.background());
}

TEST(PickerItemViewTest, HasBackgroundInPseudoFocusedState) {
  PickerItemView item_view(base::DoNothing());

  item_view.SetItemState(PickerItemView::ItemState::kPseudoFocused);

  EXPECT_TRUE(item_view.background());
}

}  // namespace
}  // namespace ash
