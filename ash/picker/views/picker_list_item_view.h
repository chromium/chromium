// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_LIST_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_LIST_ITEM_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/files/file_path.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ui {
class ImageModel;
}

namespace views {
class ImageView;
class View;
}  // namespace views

namespace ash {

class PickerBadgeView;
class PickerPreviewBubbleController;

// View for a Picker list item with text or an image as its primary contents.
// Can optionally have other parts such as a leading icon and secondary text.
class ASH_EXPORT PickerListItemView : public PickerItemView {
  METADATA_HEADER(PickerListItemView, PickerItemView)

 public:
  using AsyncBitmapResolver = HoldingSpaceImage::AsyncBitmapResolver;

  explicit PickerListItemView(SelectItemCallback select_item_callback);
  PickerListItemView(const PickerListItemView&) = delete;
  PickerListItemView& operator=(const PickerListItemView&) = delete;
  ~PickerListItemView() override;

  void SetLeadingIcon(const ui::ImageModel& icon);

  // Sets the primary text or image of the list item. This replaces any existing
  // contents in the primary container.
  void SetPrimaryText(const std::u16string& primary_text);
  void SetPrimaryImage(std::unique_ptr<views::ImageView> primary_image);

  void SetSecondaryText(const std::u16string& secondary_text);

  void SetBadgeAction(PickerActionType action);
  void SetBadgeVisible(bool visible);

  // Starts to retrieve a thumbnail preview of `file_path` to be used when the
  // item is hovered on. If `update_icon` is true, then the leading icon of this
  // item will also be updated to match the thumbnail.
  void SetPreview(PickerPreviewBubbleController* preview_bubble_controller,
                  base::FilePath file_path,
                  AsyncBitmapResolver async_bitmap_resolver,
                  bool update_icon = false);

  // views::Button:
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;

  const views::ImageView& leading_icon_view_for_testing() const {
    return *leading_icon_view_;
  }
  const views::View* primary_container_for_testing() const {
    return primary_container_;
  }
  const PickerBadgeView& trailing_badge_for_testing() const {
    return *trailing_badge_;
  }
  std::u16string GetPrimaryTextForTesting() const;
  ui::ImageModel GetPrimaryImageForTesting() const;

 private:
  void UpdateIconWithPreview();

  raw_ptr<views::ImageView> leading_icon_view_ = nullptr;

  // Contains the item's primary contents, which can be text or an image.
  raw_ptr<views::View> primary_container_ = nullptr;

  // Contains the item's secondary text if it has been set.
  raw_ptr<views::View> secondary_container_ = nullptr;

  // Contains the item's trailing badge if it has been set.
  raw_ptr<PickerBadgeView> trailing_badge_ = nullptr;

  // These are only used for file items.
  // TODO: b/344457947 - Combine the two async images by allowing the
  // placeholder image to be dynamically generated based on the size.
  std::unique_ptr<HoldingSpaceImage> async_preview_image_;
  std::unique_ptr<HoldingSpaceImage> async_preview_icon_;
  raw_ptr<PickerPreviewBubbleController> preview_bubble_controller_;
  base::CallbackListSubscription async_icon_subscription_;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_LIST_ITEM_VIEW_H_
