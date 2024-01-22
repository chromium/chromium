// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace views {
class ImageView;
class View;
}  // namespace views

namespace ash {

// View for a Picker list item with text or an image as its primary contents.
// Can optionally have other parts such as a leading icon and secondary text.
class ASH_EXPORT PickerItemView : public views::Button {
  METADATA_HEADER(PickerItemView, views::Button)

 public:
  explicit PickerItemView(views::Button::PressedCallback callback);
  PickerItemView(const PickerItemView&) = delete;
  PickerItemView& operator=(const PickerItemView&) = delete;
  ~PickerItemView() override;

  void SetLeadingIcon(const gfx::VectorIcon& icon);

  // Sets the primary text or image of the list item. This replaces any existing
  // contents in the primary container.
  void SetPrimaryText(const std::u16string& primary_text);
  void SetPrimaryImage(std::unique_ptr<views::ImageView> primary_image);

  void SetSecondaryText(const std::u16string& secondary_text);

  const views::View* leading_container_for_testing() const {
    return leading_container_;
  }
  const views::View* primary_container_for_testing() const {
    return primary_container_;
  }

 private:
  // Contains the item's leading icon if it has been set.
  raw_ptr<views::View> leading_container_ = nullptr;

  // Contains the item's primary contents, which can be text or an image.
  raw_ptr<views::View> primary_container_ = nullptr;

  // Contains the item's secondary text if it has been set.
  raw_ptr<views::View> secondary_container_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_ITEM_VIEW_H_
