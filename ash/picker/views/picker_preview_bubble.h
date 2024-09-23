// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_H_
#define ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_H_

#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/view.h"

namespace views {
class BoxLayoutView;
class ImageView;
class Label;
}

namespace ash {

class ASH_EXPORT PickerPreviewBubbleView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(PickerPreviewBubbleView, views::BubbleDialogDelegateView)

 public:
  explicit PickerPreviewBubbleView(views::View* anchor_view);
  PickerPreviewBubbleView(const PickerPreviewBubbleView&) = delete;
  PickerPreviewBubbleView& operator=(const PickerPreviewBubbleView&) = delete;

  static constexpr auto kPreviewImageSize = gfx::Size(240, 135);

  ui::ImageModel GetPreviewImage() const;
  void SetPreviewImage(ui::ImageModel image);

  bool GetLabelVisibleForTesting() const;
  std::u16string_view GetMainTextForTesting() const;

  // Sets the text of the label and makes them visible.
  void SetText(const std::u16string& main_text);
  void ClearText();

  // BubbleDialogDelegateView overrides
  void OnThemeChanged() override;
  gfx::Rect GetAnchorRect() const override;

  void Close();

 private:
  raw_ptr<views::ImageView> image_view_;

  raw_ptr<views::BoxLayoutView> box_layout_view_;
  raw_ptr<views::Label> main_label_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   PickerPreviewBubbleView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerPreviewBubbleView)

#endif  // ASH_PICKER_VIEWS_PICKER_PREVIEW_BUBBLE_H_
