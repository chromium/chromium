// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"

#include "ash/bubble/bubble_utils.h"
#include "ash/style/typography.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

std::unique_ptr<views::View> CreateNameTag(const std::u16string& title,
                                           const std::u16string& sub_title) {
  auto name_tag = std::make_unique<views::View>();
  name_tag->SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetMainAxisAlignment(views::LayoutAlignment::kStart)
      .SetCrossAxisAlignment(views::LayoutAlignment::kStart);
  name_tag->AddChildView(
      ash::bubble_utils::CreateLabel(ash::TypographyToken::kCrosButton1, title,
                                     cros_tokens::kCrosRefNeutral100));
  name_tag->AddChildView(ash::bubble_utils::CreateLabel(
      ash::TypographyToken::kCrosAnnotation2, sub_title,
      cros_tokens::kCrosSysSecondary));
  return name_tag;
}

std::unique_ptr<views::View> CreateActionMoveEditForKeyboard(Action* action) {
  auto keys = std::make_unique<views::View>();
  // Create a 2x3 table with column and row padding of 4.
  keys->SetLayoutManager(std::make_unique<views::TableLayout>())
      ->AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                  /*v_align=*/views::LayoutAlignment::kCenter,
                  /*horizontal_resize=*/1.0f,
                  /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                  /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/4)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddPaddingColumn(/*horizontal_resize=*/views::TableLayout::kFixedSize,
                        /*width=*/4)
      .AddColumn(/*h_align=*/views::LayoutAlignment::kCenter,
                 /*v_align=*/views::LayoutAlignment::kCenter,
                 /*horizontal_resize=*/1.0f,
                 /*size_type=*/views::TableLayout::ColumnSize::kUsePreferred,
                 /*fixed_width=*/0, /*min_width=*/0)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize)
      .AddPaddingRow(/*vertical_resize=*/views::TableLayout::kFixedSize,
                     /*height=*/4)
      .AddRows(1, /*vertical_resize=*/views::TableLayout::kFixedSize);
  // Column 1 row 1 is empty.
  keys->AddChildView(std::make_unique<views::View>());
  // TODO(b/270969479): Replace the hardcoded text.
  keys->AddChildView(std::make_unique<views::LabelButton>(
      views::Button::PressedCallback(), u"w"));
  // Column 3 row 1 is empty.
  keys->AddChildView(std::make_unique<views::View>());
  // TODO(b/270969479): Replace the hardcoded text.
  keys->AddChildView(std::make_unique<views::LabelButton>(
      views::Button::PressedCallback(), u"a"));
  keys->AddChildView(std::make_unique<views::LabelButton>(
      views::Button::PressedCallback(), u"s"));
  keys->AddChildView(std::make_unique<views::LabelButton>(
      views::Button::PressedCallback(), u"d"));
  return keys;
}

}  // namespace arc::input_overlay
