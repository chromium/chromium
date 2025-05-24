// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PREVIEW_BUBBLE_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PREVIEW_BUBBLE_H_

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
}  // namespace views

namespace ash {

class ASH_EXPORT QuickInsertPreviewBubbleView
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(QuickInsertPreviewBubbleView, views::BubbleDialogDelegateView)

 public:
  explicit QuickInsertPreviewBubbleView(views::View* anchor_view);
  QuickInsertPreviewBubbleView(const QuickInsertPreviewBubbleView&) = delete;
  QuickInsertPreviewBubbleView& operator=(const QuickInsertPreviewBubbleView&) =
      delete;

  static constexpr auto kPreviewImageSize = gfx::Size(240, 135);

  ui::ImageModel GetPreviewImage() const;
  void SetPreviewImage(ui::ImageModel image);

  bool GetLabelVisibleForTesting() const;
  std::u16string_view GetMainTextForTesting() const;

  // Sets the text of the label and makes them visible.
  void SetText(const std::u16string& main_text);
  void ClearText();

  // BubbleDialogDelegateView overrides
  gfx::Rect GetAnchorRect() const override;

  void Close();

 private:
  raw_ptr<views::ImageView> image_view_;

  raw_ptr<views::BoxLayoutView> box_layout_view_;
  raw_ptr<views::Label> main_label_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT,
                   QuickInsertPreviewBubbleView,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::QuickInsertPreviewBubbleView)

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_PREVIEW_BUBBLE_H_
