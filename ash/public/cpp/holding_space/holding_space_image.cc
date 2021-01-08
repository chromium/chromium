// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <algorithm>
#include <map>

#include "ash/public/cpp/file_icon_util.h"
#include "ash/public/cpp/holding_space/holding_space_color_provider.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"

namespace ash {

namespace {

// Appearance.
constexpr int kFileTypeIconSize = 20;

// Whether image invalidation should be done without a delay. May be set in
// tests.
bool g_use_zero_invalidation_delay_for_testing = false;

// EmptyImageSkiaSource --------------------------------------------------------

class EmptyImageSkiaSource : public gfx::CanvasImageSource {
 public:
  explicit EmptyImageSkiaSource(const gfx::Size& size)
      : gfx::CanvasImageSource(size) {}

  EmptyImageSkiaSource(const EmptyImageSkiaSource&) = delete;
  EmptyImageSkiaSource& operator=(const EmptyImageSkiaSource&) = delete;
  ~EmptyImageSkiaSource() override = default;

 private:
  // gfx::CanvasImageSource:
  void Draw(gfx::Canvas* canvas) override {}  // Draw nothing.
};

// Helpers ---------------------------------------------------------------------

// Creates an empty image of the specified `size`.
gfx::ImageSkia CreateEmptyImageSkia(const gfx::Size& size) {
  return gfx::ImageSkia(std::make_unique<EmptyImageSkiaSource>(size), size);
}

// Creates an image to represent the file type of the specified `file_path`.
gfx::ImageSkia CreateFileTypeImageSkia(const base::FilePath& file_path,
                                       bool is_folder,
                                       const gfx::Size& size) {
  gfx::ImageSkia file_type_icon;
  if (is_folder) {
    file_type_icon = gfx::CreateVectorIcon(
        chromeos::kFiletypeFolderIcon, kFileTypeIconSize,
        HoldingSpaceColorProvider::Get()->GetFileIconColor());
  } else {
    file_type_icon = GetIconForPath(
        file_path, HoldingSpaceColorProvider::Get()->GetFileIconColor());
  }
  // Superimpose the `file_type_icon` over an empty image in order to center it
  // within the image at a fixed size.
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(
      CreateEmptyImageSkia(size), file_type_icon);
}

}  // namespace

// HoldingSpaceImage::ImageSkiaSource ------------------------------------------

class HoldingSpaceImage::ImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  ImageSkiaSource(const base::WeakPtr<HoldingSpaceImage>& host,
                  const gfx::ImageSkia& placeholder)
      : host_(host), placeholder_(placeholder) {}

  ImageSkiaSource(const ImageSkiaSource&) = delete;
  ImageSkiaSource& operator=(const ImageSkiaSource&) = delete;
  ~ImageSkiaSource() override = default;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    if (host_)
      host_->LoadBitmap(scale);

    // Use `placeholder_` while we wait for the async bitmap to resolve.
    return placeholder_.GetRepresentation(scale);
  }

  const base::WeakPtr<HoldingSpaceImage> host_;
  const gfx::ImageSkia placeholder_;
};

// HoldingSpaceImage -----------------------------------------------------------

HoldingSpaceImage::HoldingSpaceImage(const gfx::Size& max_size,
                                     const base::FilePath& backing_file_path,
                                     AsyncBitmapResolver async_bitmap_resolver)
    : max_size_(max_size),
      backing_file_path_(backing_file_path),
      async_bitmap_resolver_(async_bitmap_resolver) {
  // Use an empty `placeholder_` until a bitmap is asynchronously returned.
  placeholder_ = CreateEmptyImageSkia(max_size_);
  CreateImageSkia();
}

HoldingSpaceImage::~HoldingSpaceImage() = default;

// static
gfx::Size HoldingSpaceImage::GetMaxSizeForType(HoldingSpaceItem::Type type) {
  gfx::Size size;
  switch (type) {
    case HoldingSpaceItem::Type::kDownload:
    case HoldingSpaceItem::Type::kNearbyShare:
    case HoldingSpaceItem::Type::kPinnedFile:
      size = gfx::Size(kHoldingSpaceChipIconSize, kHoldingSpaceChipIconSize);
      break;
    case HoldingSpaceItem::Type::kScreenRecording:
    case HoldingSpaceItem::Type::kScreenshot:
      size = kHoldingSpaceScreenCaptureSize;
      break;
  }
  // To avoid pixelation, ensure that the holding space image size is at least
  // as large as the tray icon preview size. The image will be scaled down
  // elsewhere if needed.
  size.SetToMax(gfx::Size(kHoldingSpaceTrayIconPreviewSize,
                          kHoldingSpaceTrayIconPreviewSize));
  return size;
}

// static
void HoldingSpaceImage::SetUseZeroInvalidationDelayForTesting(bool value) {
  g_use_zero_invalidation_delay_for_testing = value;
}

bool HoldingSpaceImage::operator==(const HoldingSpaceImage& rhs) const {
  return gfx::BitmapsAreEqual(*image_skia_.bitmap(), *rhs.image_skia_.bitmap());
}

base::CallbackListSubscription HoldingSpaceImage::AddImageSkiaChangedCallback(
    CallbackList::CallbackType callback) const {
  return callback_list_.Add(std::move(callback));
}

void HoldingSpaceImage::LoadBitmap(float scale) {
  async_bitmap_resolver_.Run(
      backing_file_path_, gfx::ScaleToCeiledSize(image_skia_.size(), scale),
      base::BindOnce(&HoldingSpaceImage::OnBitmapLoaded,
                     weak_factory_.GetWeakPtr(), backing_file_path_, scale));
}

void HoldingSpaceImage::OnBitmapLoaded(const base::FilePath& file_path,
                                       float scale,
                                       const SkBitmap* bitmap,
                                       base::File::Error error) {
  if (!bitmap) {
    DCHECK_NE(error, base::File::FILE_OK);
    if (backing_file_path_ != file_path) {
      // Retry load if the backing file path has changed while the image load
      // was in progress.
      LoadBitmap(scale);
    } else if (async_bitmap_resolver_error_ != error) {
      // A new `error` may mean a better file type image can be displayed. The
      // `error`, for example, may indicate that the file is in fact a folder in
      // which case there is a more appropriate icon that can be shown. Notify
      // subscribers to invalidate themselves.
      async_bitmap_resolver_error_ = error;
      callback_list_.Notify();
    }
    return;
  }

  DCHECK_EQ(error, base::File::FILE_OK);
  async_bitmap_resolver_error_ = base::File::FILE_OK;

  // Force invalidate `image_skia_` for `scale` so that it will request the
  // updated `gfx::ImageSkiaRep` at next access.
  image_skia_.RemoveRepresentation(scale);
  image_skia_.AddRepresentation(gfx::ImageSkiaRep(*bitmap, scale));
  image_skia_.RemoveUnsupportedRepresentationsForScale(scale);

  // Update the placeholder image, so the newly loaded representation becomes
  // the default for any `ImageSkia` instances created when the holding space
  // image gets invalidated.
  placeholder_.RemoveRepresentation(scale);
  placeholder_.AddRepresentation(gfx::ImageSkiaRep(*bitmap, scale));
  placeholder_.RemoveUnsupportedRepresentationsForScale(scale);

  callback_list_.Notify();
}

gfx::ImageSkia HoldingSpaceImage::GetImageSkia(
    const base::Optional<gfx::Size>& opt_size) const {
  const gfx::Size size = opt_size.value_or(max_size_);

  // Requested `size` must be less than or equal to `max_size_` to avoid
  // pixelation that would otherwise occur due to upscaling.
  DCHECK_LE(size.height(), max_size_.height());
  DCHECK_LE(size.width(), max_size_.width());

  // Requested `size` must be greater than the file type icon size in order for
  // the image representing file type to render correctly.
  DCHECK_GT(size.height(), kFileTypeIconSize);
  DCHECK_GT(size.width(), kFileTypeIconSize);

  // When an error occurs, fallback to an image representing file type.
  if (async_bitmap_resolver_error_ &&
      async_bitmap_resolver_error_ != base::File::FILE_OK) {
    const bool is_folder =
        async_bitmap_resolver_error_ == base::File::FILE_ERROR_NOT_A_FILE;
    return CreateFileTypeImageSkia(backing_file_path_, is_folder, size);
  }

  // Short-circuit resizing logic.
  if (image_skia_.size() == size)
    return image_skia_;

  gfx::ImageSkia image_skia(image_skia_);

  // Resize.
  const float scale_x = size.width() / static_cast<float>(image_skia.width());
  const float scale_y = size.height() / static_cast<float>(image_skia.height());
  const float scale = std::max(scale_x, scale_y);
  DCHECK_LE(scale, 1.f);  // Upscaling would result in pixelation.
  gfx::Size scaled_size = gfx::ScaleToCeiledSize(image_skia.size(), scale);
  image_skia = gfx::ImageSkiaOperations::CreateResizedImage(
      image_skia, skia::ImageOperations::ResizeMethod::RESIZE_BEST,
      scaled_size);

  // Crop.
  gfx::Rect cropped_bounds(image_skia.size());
  cropped_bounds.ClampToCenteredSize(size);
  return gfx::ImageSkiaOperations::ExtractSubset(image_skia, cropped_bounds);
}

void HoldingSpaceImage::Invalidate() {
  if (invalidate_timer_.IsRunning())
    return;

  // Schedule an invalidation task with a delay to reduce number of image loads
  // when multiple image invalidations are requested in quick succession. The
  // delay is selected somewhat arbitrarily to be non trivial but still not
  // easily noticable by the user.
  invalidate_timer_.Start(FROM_HERE,
                          g_use_zero_invalidation_delay_for_testing
                              ? base::TimeDelta()
                              : base::TimeDelta::FromMilliseconds(250),
                          base::BindOnce(&HoldingSpaceImage::OnInvalidateTimer,
                                         base::Unretained(this)));
}

void HoldingSpaceImage::UpdateBackingFilePath(const base::FilePath& file_path) {
  backing_file_path_ = file_path;
}

bool HoldingSpaceImage::FireInvalidateTimerForTesting() {
  if (!invalidate_timer_.IsRunning())
    return false;
  invalidate_timer_.FireNow();
  return true;
}

void HoldingSpaceImage::OnInvalidateTimer() {
  // Invalidate the existing pointers to:
  // *   Invalidate previous `image_skia_`'s host pointer, and prevent it from
  //     requesting bitmap loads.
  // *   Prevent pending bitmap request callbacks from running.
  weak_factory_.InvalidateWeakPtrs();

  CreateImageSkia();

  callback_list_.Notify();
}

void HoldingSpaceImage::CreateImageSkia() {
  image_skia_ =
      gfx::ImageSkia(std::make_unique<ImageSkiaSource>(
                         /*host=*/weak_factory_.GetWeakPtr(), placeholder_),
                     max_size_);
}

}  // namespace ash
