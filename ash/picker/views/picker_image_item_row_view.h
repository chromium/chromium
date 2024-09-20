// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_ROW_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_ROW_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
class ImageView;
class View;
}  // namespace views

namespace ash {

class PickerImageItemView;

// Container view for a single row of image items in a section.
class ASH_EXPORT PickerImageItemRowView
    : public views::BoxLayoutView,
      public PickerTraversableItemContainer {
  METADATA_HEADER(PickerImageItemRowView, views::BoxLayoutView)

 public:
  explicit PickerImageItemRowView(
      base::RepeatingClosure more_items_button = {},
      std::u16string more_items_accessible_name = u"");
  PickerImageItemRowView(const PickerImageItemRowView&) = delete;
  PickerImageItemRowView& operator=(const PickerImageItemRowView&) = delete;
  ~PickerImageItemRowView() override;

  void SetLeadingIcon(const ui::ImageModel& icon);
  PickerImageItemView* AddImageItem(
      std::unique_ptr<PickerImageItemView> image_item);

  // views::BoxLayoutView:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  views::View::Views GetItems() const;
  [[nodiscard]] base::CallbackListSubscription AddItemsChangedCallback(
      views::PropertyChangedCallback callback);

  views::ImageButton* GetMoreItemsButtonForTesting() {
    return more_items_button_;
  }

 private:
  views::View* GetLeftmostItem();

  raw_ptr<views::ImageView> leading_icon_view_ = nullptr;
  raw_ptr<views::View> items_container_ = nullptr;
  raw_ptr<views::ImageButton> more_items_button_ = nullptr;
  base::RepeatingClosureList on_items_changed_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerImageItemRowView, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerImageItemRowView)

#endif  // ASH_PICKER_VIEWS_PICKER_IMAGE_ITEM_ROW_VIEW_H_
