// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_LIST_ITEM_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_LIST_ITEM_VIEW_H_

#include <optional>
#include <string>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/views/picker_item_view.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class FilePath;
}

namespace ui {
class ImageModel;
}

namespace views {
class ImageView;
class Label;
class View;
}  // namespace views

namespace ash {

class PickerBadgeView;
class PickerPreviewBubbleController;
class PickerShortcutHintView;

// View for a Picker list item with text or an image as its primary contents.
// Can optionally have other parts such as a leading icon and secondary text.
class ASH_EXPORT PickerListItemView : public PickerItemView {
  METADATA_HEADER(PickerListItemView, PickerItemView)

 public:
  using AsyncBitmapResolver = HoldingSpaceImage::AsyncBitmapResolver;
  using FileInfoResolver =
      base::OnceCallback<std::optional<base::File::Info>()>;

  explicit PickerListItemView(SelectItemCallback select_item_callback);
  PickerListItemView(const PickerListItemView&) = delete;
  PickerListItemView& operator=(const PickerListItemView&) = delete;
  ~PickerListItemView() override;

  // PickerItemView:
  void SetItemState(ItemState item_state) override;

  void SetLeadingIcon(const ui::ImageModel& icon,
                      std::optional<gfx::Size> icon_size = std::nullopt);

  // Sets the primary text. This replaces any existing contents in the primary
  // container.
  void SetPrimaryText(const std::u16string& primary_text);

  // Sets the primary image. This replaces any existing contents in the primary
  // container. `available_width` is the available width for this list item
  // (including any leading icons). The image will be resized to fill
  // `available_width`, while maintaining a fixed height and aspect ratio by
  // cropping out any excess.
  void SetPrimaryImage(const ui::ImageModel& primary_image,
                       int available_width);

  void SetSecondaryText(const std::u16string& secondary_text);

  void SetShortcutHintView(
      std::unique_ptr<PickerShortcutHintView> shortcut_hint_view);

  void SetBadgeAction(PickerActionType action);
  void SetBadgeVisible(bool visible);

  // Starts to retrieve a thumbnail preview of `file_path` to be used when the
  // item is hovered on. If `update_icon` is true, then the leading icon of this
  // item will also be updated to match the thumbnail.
  void SetPreview(PickerPreviewBubbleController* preview_bubble_controller,
                  FileInfoResolver get_file_info,
                  const base::FilePath& file_path,
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
  const PickerShortcutHintView* shortcut_hint_view_for_testing() const {
    return shortcut_hint_view_;
  }
  const PickerBadgeView& trailing_badge_for_testing() const {
    return *trailing_badge_;
  }
  std::u16string GetPrimaryTextForTesting() const;
  ui::ImageModel GetPrimaryImageForTesting() const;
  std::u16string_view GetSecondaryTextForTesting() const;

 private:
  void UpdateIconWithPreview();
  std::u16string GetAccessibilityLabel() const;
  void UpdateAccessibleName();
  void OnFileInfoResolved(std::optional<base::File::Info> info);

  void ShowPreview();
  void HidePreview();

  raw_ptr<views::ImageView> leading_icon_view_ = nullptr;

  // Contains the item's primary contents, which can be text or an image.
  raw_ptr<views::View> primary_container_ = nullptr;
  raw_ptr<views::Label> primary_label_ = nullptr;

  // Contains the item's secondary text if it has been set.
  raw_ptr<views::View> secondary_container_ = nullptr;
  raw_ptr<views::Label> secondary_label_ = nullptr;

  // Contains the item's shortcut hint if it has been set.
  raw_ptr<views::View> shortcut_hint_container_ = nullptr;
  raw_ptr<PickerShortcutHintView> shortcut_hint_view_ = nullptr;

  // Contains the item's trailing badge if it has been set.
  raw_ptr<PickerBadgeView> trailing_badge_ = nullptr;
  PickerActionType badge_action_ = PickerActionType::kDo;

  // These are only used for file items.
  // TODO: b/344457947 - Combine the two async images by allowing the
  // placeholder image to be dynamically generated based on the size.
  std::unique_ptr<HoldingSpaceImage> async_preview_image_;
  std::unique_ptr<HoldingSpaceImage> async_preview_icon_;
  base::FilePath file_path_;
  std::optional<base::File::Info> file_info_;
  raw_ptr<PickerPreviewBubbleController> preview_bubble_controller_;
  base::CallbackListSubscription async_icon_subscription_;

  base::WeakPtrFactory<PickerListItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_LIST_ITEM_VIEW_H_
