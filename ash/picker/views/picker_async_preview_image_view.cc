// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_async_preview_image_view.h"

#include <utility>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/background.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {
namespace {

constexpr int kCornerRadius = 8;

}

PickerAsyncPreviewImageView::PickerAsyncPreviewImageView(
    base::FilePath path,
    const gfx::Size& size,
    AsyncBitmapResolver async_bitmap_resolver)
    : async_preview_image_(size,
                           std::move(path),
                           std::move(async_bitmap_resolver)) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysAppBaseShaded, kCornerRadius));

  // base::Unretained is safe here since `async_preview_subscription_` is a
  // member. During destruction, `async_icon_subscription_` will be destroyed
  // before the other members, so the callback is guaranteed to be safe.
  async_preview_subscription_ =
      async_preview_image_.AddImageSkiaChangedCallback(
          base::BindRepeating(&PickerAsyncPreviewImageView::OnImageSkiaChanged,
                              base::Unretained(this)));

  // Use the initial placeholder image.
  OnImageSkiaChanged();
}

PickerAsyncPreviewImageView::~PickerAsyncPreviewImageView() = default;

void PickerAsyncPreviewImageView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  views::ImageView::OnBoundsChanged(previous_bounds);

  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(GetImageBounds()),
                    SkIntToScalar(kCornerRadius), SkIntToScalar(kCornerRadius));
  SetClipPath(path);
}

void PickerAsyncPreviewImageView::OnImageSkiaChanged() {
  SetImage(ui::ImageModel::FromImageSkia(async_preview_image_.GetImageSkia()));
}

BEGIN_METADATA(PickerAsyncPreviewImageView)
END_METADATA

}  // namespace ash
