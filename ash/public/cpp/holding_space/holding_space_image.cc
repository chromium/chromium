// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <map>

#include "base/bind.h"
#include "base/callback.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"

namespace ash {

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

HoldingSpaceImage::HoldingSpaceImage(const gfx::ImageSkia& placeholder,
                                     AsyncBitmapResolver async_bitmap_resolver)
    : placeholder_(placeholder), async_bitmap_resolver_(async_bitmap_resolver) {
  CreateImageSkia();
}

HoldingSpaceImage::~HoldingSpaceImage() = default;

bool HoldingSpaceImage::operator==(const HoldingSpaceImage& rhs) const {
  return gfx::BitmapsAreEqual(*image_skia_.bitmap(), *rhs.image_skia_.bitmap());
}

base::CallbackListSubscription HoldingSpaceImage::AddImageSkiaChangedCallback(
    CallbackList::CallbackType callback) const {
  return callback_list_.Add(std::move(callback));
}

void HoldingSpaceImage::LoadBitmap(float scale) {
  async_bitmap_resolver_.Run(gfx::ScaleToCeiledSize(image_skia_.size(), scale),
                             scale,
                             base::BindOnce(&HoldingSpaceImage::OnBitmapLoaded,
                                            weak_factory_.GetWeakPtr(), scale));
}

void HoldingSpaceImage::OnBitmapLoaded(float scale, const SkBitmap* bitmap) {
  if (!bitmap)
    return;

  // Force invalidate `image_skia_` for `scale` so that it will request the
  // updated `gfx::ImageSkiaRep` at next access.
  image_skia_.RemoveRepresentation(scale);
  image_skia_.AddRepresentation(gfx::ImageSkiaRep(*bitmap, scale));
  image_skia_.RemoveUnsupportedRepresentationsForScale(scale);

  // Update the placeholder image, so the newly loaded representation becomes
  // the default for any `ImageSkia` instances created when the holding space
  // image gets refreshed.
  placeholder_.RemoveRepresentation(scale);
  placeholder_.AddRepresentation(gfx::ImageSkiaRep(*bitmap, scale));
  placeholder_.RemoveUnsupportedRepresentationsForScale(scale);

  callback_list_.Notify();
}

void HoldingSpaceImage::Invalidate() {
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
