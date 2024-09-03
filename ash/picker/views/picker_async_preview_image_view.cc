// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_async_preview_image_view.h"

#include <utility>

#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/image_util.h"
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

HoldingSpaceImage::PlaceholderImageSkiaResolver
CreateEmptyPlaceholderImageSkiaResolver() {
  return base::BindRepeating([](const base::FilePath& backing_file_path,
                                const gfx::Size& size,
                                const std::optional<bool>& dark_background,
                                const std::optional<bool>& is_folder) {
    return image_util::CreateEmptyImage(size);
  });
}
}

PickerAsyncPreviewImageView::PickerAsyncPreviewImageView(
    base::FilePath path,
    const gfx::Size& max_size,
    AsyncBitmapResolver async_bitmap_resolver)
    : max_size_(max_size),
      async_preview_image_(max_size_,
                           std::move(path),
                           std::move(async_bitmap_resolver),
                           CreateEmptyPlaceholderImageSkiaResolver()) {
  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysAppBaseShaded, kCornerRadius));

  // base::Unretained is safe here since `async_preview_subscription_` is a
  // member. During destruction, `async_icon_subscription_` will be destroyed
  // before the other members, so the callback is guaranteed to be safe.
  async_preview_subscription_ =
      async_preview_image_.AddImageSkiaChangedCallback(
          base::BindRepeating(&PickerAsyncPreviewImageView::UpdateImageSkia,
                              base::Unretained(this)));

  // Use the initial placeholder image.
  UpdateImageSkia();
}

PickerAsyncPreviewImageView::~PickerAsyncPreviewImageView() = default;

void PickerAsyncPreviewImageView::OnBoundsChanged(
    const gfx::Rect& previous_bounds) {
  UpdateImageSkia();

  SkPath path;
  path.addRoundRect(gfx::RectToSkRect(GetLocalBounds()),
                    SkIntToScalar(kCornerRadius), SkIntToScalar(kCornerRadius));
  SetClipPath(path);

  views::ImageView::OnBoundsChanged(previous_bounds);
}

gfx::Size PickerAsyncPreviewImageView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Calculate the height to retain aspect ratio.
  const int preferred_width =
      views::ImageView::CalculatePreferredSize(available_size).width();
  const int height =
      max_size_.width() == 0
          ? 0
          : (preferred_width * max_size_.height()) / max_size_.width();
  return gfx::Size(preferred_width, height);
}

void PickerAsyncPreviewImageView::UpdateImageSkia() {
  const gfx::Size local_bounds = GetLocalBounds().size();
  SetImage(ui::ImageModel::FromImageSkia(async_preview_image_.GetImageSkia(
      local_bounds.IsEmpty() ? std::nullopt
                             : std::make_optional(local_bounds))));
}

BEGIN_METADATA(PickerAsyncPreviewImageView)
END_METADATA

}  // namespace ash
