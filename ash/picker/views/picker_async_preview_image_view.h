// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_ASYNC_PREVIEW_IMAGE_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_ASYNC_PREVIEW_IMAGE_VIEW_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/metadata/view_factory.h"

namespace gfx {
class Size;
}

namespace ash {

// An ImageView that fetches the thumbnail preview of a file asynchronously and
// updates itself when the preview is fetched.
class ASH_EXPORT PickerAsyncPreviewImageView : public views::ImageView {
  METADATA_HEADER(PickerAsyncPreviewImageView, views::ImageView)

 public:
  using AsyncBitmapResolver = HoldingSpaceImage::AsyncBitmapResolver;

  explicit PickerAsyncPreviewImageView(
      base::FilePath path,
      const gfx::Size& max_size,
      AsyncBitmapResolver async_bitmap_resolver);
  PickerAsyncPreviewImageView(const PickerAsyncPreviewImageView&) = delete;
  PickerAsyncPreviewImageView& operator=(const PickerAsyncPreviewImageView&) =
      delete;
  ~PickerAsyncPreviewImageView() override;

  // views::ImageView:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

 private:
  // Resizes and crops the preview image to the bounds of the view.
  void UpdateImageSkia();

  gfx::Size max_size_;
  HoldingSpaceImage async_preview_image_;
  base::CallbackListSubscription async_preview_subscription_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, PickerAsyncPreviewImageView, views::ImageView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::PickerAsyncPreviewImageView)

#endif  // ASH_PICKER_VIEWS_PICKER_ASYNC_PREVIEW_IMAGE_VIEW_H_
