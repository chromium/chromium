// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"

namespace ash {

namespace {

// Whether image invalidation should be done without a delay. May be set in
// tests.
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

HoldingSpaceImage::HoldingSpaceImage(const base::FilePath& backing_file_path,
                                     const gfx::ImageSkia& placeholder,
                                     AsyncBitmapResolver async_bitmap_resolver)
    : backing_file_path_(backing_file_path),
      placeholder_(placeholder),
      async_bitmap_resolver_(async_bitmap_resolver) {
  CreateImageSkia();
}

HoldingSpaceImage::~HoldingSpaceImage() = default;

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
      scale,
      base::BindOnce(&HoldingSpaceImage::OnBitmapLoaded,
                     weak_factory_.GetWeakPtr(), backing_file_path_, scale));
}

void HoldingSpaceImage::OnBitmapLoaded(const base::FilePath& file_path,
                                       float scale,
                                       const SkBitmap* bitmap) {
  if (!bitmap) {
    // Retry load if the backing file path has changed while the image load was
    // in progress.
    if (backing_file_path_ != file_path)
      LoadBitmap(scale);
    return;
  }

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
                     placeholder_.size());
}

}  // namespace ash
