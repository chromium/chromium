// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/shelf_test_base.h"

#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/test/ash_test_util.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

ShelfTestBase::ShelfTestBase() = default;

ShelfTestBase::~ShelfTestBase() = default;

void ShelfTestBase::SetUp() {
  AshTestBase::SetUp();
  UpdateShelfRelatedMembers();
}

void ShelfTestBase::TearDown() {
  // When the test is completed, the page flip timer should be idle.
  EXPECT_FALSE(scrollable_shelf_view_->IsPageFlipTimerBusyForTest());

  AshTestBase::TearDown();
}

void ShelfTestBase::UpdateShelfRelatedMembers() {
  scrollable_shelf_view_ = GetPrimaryShelf()
                               ->shelf_widget()
                               ->hotseat_widget()
                               ->scrollable_shelf_view();
  shelf_view_ = scrollable_shelf_view_->shelf_view();
  test_api_ =
      std::make_unique<ShelfViewTestAPI>(scrollable_shelf_view_->shelf_view());
  test_api_->SetAnimationDuration(base::Milliseconds(1));
}

void ShelfTestBase::PopulateAppShortcut(int number,
                                        bool use_alternative_color) {
  for (int i = 0; i < number; i++)
    AddAppShortcutWithIconColor(
        TYPE_PINNED_APP, use_alternative_color
                             ? icon_color_generator_.GetAlternativeColor()
                             : icon_color_generator_.default_color());
}

void ShelfTestBase::AddAppShortcutsUntilOverflow(bool use_alternative_color) {
  while (scrollable_shelf_view_->layout_strategy_for_test() ==
         ScrollableShelfView::kNotShowArrowButtons) {
    AddAppShortcutWithIconColor(
        TYPE_PINNED_APP, use_alternative_color
                             ? icon_color_generator_.GetAlternativeColor()
                             : icon_color_generator_.default_color());
  }
}

ShelfItem ShelfTestBase::AddWebAppShortcut() {
  const int icon_dimension = 28;
  const int badge_icon_dimension = 18;
  ShelfItem item = ShelfTestUtil::AddWebAppShortcut(
      "shortcut_id", true,
      gfx::test::CreateImageSkia(icon_dimension, SK_ColorRED),
      gfx::test::CreateImageSkia(badge_icon_dimension, SK_ColorCYAN));
  return item;
}

ShelfID ShelfTestBase::AddAppShortcutWithIconColor(ShelfItemType item_type,
                                                   SkColor color) {
  ShelfItem item = ShelfTestUtil::AddAppShortcutWithIcon(
      base::NumberToString(id_++), item_type,
      CreateSolidColorTestImage(gfx::Size(1, 1), color));

  // Wait for shelf view's bounds animation to end. Otherwise the scrollable
  // shelf's bounds are not updated yet.
  test_api_->RunMessageLoopUntilAnimationsDone();

  return item.id;
}

}  // namespace ash
