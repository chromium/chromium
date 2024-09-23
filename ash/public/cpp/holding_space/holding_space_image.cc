// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <algorithm>
#include <map>
#include <set>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/image_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "chromeos/ui/base/file_icon_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace {

// Appearance.
constexpr int kFileTypeIconSize = 20;

// Whether image invalidation should be done without delay.
bool g_use_zero_invalidation_delay_for_testing = false;

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
    : HoldingSpaceImage(max_size,
                        backing_file_path,
                        async_bitmap_resolver,
                        CreateDefaultPlaceholderImageSkiaResolver()) {}

HoldingSpaceImage::HoldingSpaceImage(
    const gfx::Size& max_size,
    const base::FilePath& backing_file_path,
    AsyncBitmapResolver async_bitmap_resolver,
    PlaceholderImageSkiaResolver placeholder_image_skia_resolver)
    : max_size_(max_size),
      backing_file_path_(backing_file_path),
      async_bitmap_resolver_(async_bitmap_resolver),
      placeholder_image_skia_resolver_(placeholder_image_skia_resolver) {
  placeholder_ = placeholder_image_skia_resolver_.Run(
      backing_file_path_, max_size_, /*dark_background=*/std::nullopt,
      /*is_folder=*/std::nullopt);
  CreateImageSkia();
}

HoldingSpaceImage::~HoldingSpaceImage() = default;

// static
HoldingSpaceImage::PlaceholderImageSkiaResolver
HoldingSpaceImage::CreateDefaultPlaceholderImageSkiaResolver(
    bool use_light_mode_as_default) {
  return base::BindRepeating(
      [](bool use_light_mode_as_default,
         const base::FilePath& backing_file_path, const gfx::Size& size,
         const std::optional<bool>& dark_background,
         const std::optional<bool>& is_folder) {
        // The requested image `size` should be >= `kFileTypeIconSize` to
        // give the `file_type_icon` generated below enough space to fully
        // paint.
        DCHECK_GE(size.height(), kFileTypeIconSize);
        DCHECK_GE(size.width(), kFileTypeIconSize);

        const gfx::ImageSkia file_type_icon =
            is_folder.value_or(false)
                ? chromeos::GetIconFromType(
                      chromeos::IconType::kFolder,
                      dark_background.value_or(!use_light_mode_as_default))
                : chromeos::GetIconForPath(
                      backing_file_path,
                      dark_background.value_or(!use_light_mode_as_default));

        return gfx::ImageSkiaOperations::CreateSuperimposedImage(
            image_util::CreateEmptyImage(size), file_type_icon);
      },
      use_light_mode_as_default);
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
    } else {
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
    const std::optional<gfx::Size>& opt_size,
    bool dark_background) const {
  const gfx::Size size = opt_size.value_or(max_size_);

  // Requested `size` must be less than or equal to `max_size_` to avoid
  // pixelation that would otherwise occur due to upscaling.
  DCHECK_LE(size.height(), max_size_.height());
  DCHECK_LE(size.width(), max_size_.width());

  // When an error occurs, fallback to a resolved placeholder image.
  if (async_bitmap_resolver_error_ &&
      async_bitmap_resolver_error_ != base::File::FILE_OK) {
    return placeholder_image_skia_resolver_.Run(
        backing_file_path_, size, dark_background,
        /*is_folder=*/async_bitmap_resolver_error_ ==
            base::File::FILE_ERROR_NOT_A_FILE);
  }

  // Short-circuit resizing logic.
  if (image_skia_.size() == size)
    return image_skia_;

  return image_util::ResizeAndCropImage(image_skia_, size);
}

void HoldingSpaceImage::Invalidate() {
  if (invalidate_timer_.IsRunning())
    return;

  // Schedule an invalidation task with a delay to reduce number of image loads
  // when multiple image invalidations are requested in quick succession. The
  // delay is selected somewhat arbitrarily to be non trivial but still not
  // easily noticeable by the user.
  invalidate_timer_.Start(FROM_HERE,
                          g_use_zero_invalidation_delay_for_testing
                              ? base::TimeDelta()
                              : base::Milliseconds(250),
                          base::BindOnce(&HoldingSpaceImage::OnInvalidateTimer,
                                         base::Unretained(this)));
}

void HoldingSpaceImage::UpdateBackingFilePath(const base::FilePath& file_path) {
  backing_file_path_ = file_path;
}

bool HoldingSpaceImage::UsingPlaceholder() const {
  return !async_bitmap_resolver_error_ ||
         *async_bitmap_resolver_error_ != base::File::FILE_OK;
}

bool HoldingSpaceImage::FireInvalidateTimerForTesting() {
  if (!invalidate_timer_.IsRunning())
    return false;
  invalidate_timer_.FireNow();
  return true;
}

void HoldingSpaceImage::OnInvalidateTimer() {
  // Invalidate the existing pointers to:
  // *   Invalidate the previous `image_skia_`'s host pointer, and prevent it
  //     from requesting bitmap loads.
  // *   Prevent pending bitmap request callbacks from running.
  weak_factory_.InvalidateWeakPtrs();

  // Cache scales for which the previous `image_skia_` has reps.
  std::set<float> scales;
  for (const gfx::ImageSkiaRep& image_rep : image_skia_.image_reps())
    scales.insert(image_rep.scale());

  // Create a new `image_skia_`.
  CreateImageSkia();

  // If the `async_bitmap_resolver_` has not previously failed, notify
  // subscribers that the `image_skia_` has been invalidated. A new bitmap will
  // be asynchronously resolved at the next `GetImageSkia()` invocation.
  if (!async_bitmap_resolver_error_ ||
      async_bitmap_resolver_error_ == base::File::FILE_OK) {
    callback_list_.Notify();
    return;
  }

  // If the `async_bitmap_resolver_` failed previously, force asynchronous
  // resolution of new bitmaps. Failure to do so would cause `GetImageSkia()`
  // invocations to continue returning placeholders. Note that subscribers will
  // be notified of `image_skia_` invalidation when the `async_bitmap_resolver_`
  // returns.
  DCHECK(!scales.empty());
  for (const float& scale : scales)
    image_skia_.GetRepresentation(scale);
}

void HoldingSpaceImage::CreateImageSkia() {
  image_skia_ =
      gfx::ImageSkia(std::make_unique<ImageSkiaSource>(
                         /*host=*/weak_factory_.GetWeakPtr(), placeholder_),
                     max_size_);
}

}  // namespace ash
