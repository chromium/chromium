// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_view.h"

#include <string>
#include <utility>

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/test_layout_provider.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

using ::testing::Eq;
using ::testing::Property;
using ::testing::SizeIs;

using PickerItemViewTest = AshTestBase;

TEST_F(PickerItemViewTest, SetsPrimaryText) {
  PickerItemView item_view{views::Button::PressedCallback()};

  const std::u16string kPrimaryText = u"Item";
  item_view.SetPrimaryText(kPrimaryText);

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  const views::View* primary_label =
      item_view.primary_container_for_testing()->children()[0];
  ASSERT_TRUE(views::IsViewClass<views::Label>(primary_label));
  EXPECT_THAT(views::AsViewClass<views::Label>(primary_label),
              Property(&views::Label::GetText, Eq(kPrimaryText)));
}

TEST_F(PickerItemViewTest, SetsPrimaryImage) {
  PickerItemView item_view{views::Button::PressedCallback()};

  item_view.SetPrimaryImage(std::make_unique<views::ImageView>());

  ASSERT_THAT(item_view.primary_container_for_testing()->children(), SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<views::ImageView>(
      item_view.primary_container_for_testing()->children()[0]));
}

TEST_F(PickerItemViewTest, SetsLeadingIcon) {
  PickerItemView item_view{views::Button::PressedCallback()};

  item_view.SetLeadingIcon(kImeMenuEmoticonIcon);

  ASSERT_THAT(item_view.leading_container_for_testing()->children(), SizeIs(1));
  EXPECT_TRUE(views::IsViewClass<views::ImageView>(
      item_view.leading_container_for_testing()->children()[0]));
}

}  // namespace
}  // namespace ash
