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
  ImageSkiaSource(HoldingSpaceImage* owner,
                  const gfx::ImageSkia& placeholder,
                  AsyncBitmapResolver async_bitmap_resolver)
      : owner_(owner),
        placeholder_(placeholder),
        async_bitmap_resolver_(async_bitmap_resolver) {}

  ImageSkiaSource(const ImageSkiaSource&) = delete;
  ImageSkiaSource& operator=(const ImageSkiaSource&) = delete;
  ~ImageSkiaSource() override = default;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    // Use a cached representation when possible.
    if (base::Contains(cache_, scale))
      return cache_[scale].GetRepresentation(scale);

    // When missing the cache, asynchronously resolve the bitmap for `scale`.
    async_bitmap_resolver_.Run(
        gfx::ScaleToCeiledSize(placeholder_.size(), scale),
        base::BindOnce(&ImageSkiaSource::CacheImageForScale,
                       weak_factory_.GetWeakPtr(), scale));

    // Use `placeholder_` while we wait for the async bitmap to resolve.
    return placeholder_.GetRepresentation(scale);
  }

  void CacheImageForScale(float scale, const SkBitmap* bitmap) {
    if (bitmap) {
      cache_[scale].AddRepresentation(gfx::ImageSkiaRep(*bitmap, scale));
      owner_->NotifyUpdated(scale);
    }
  }

  HoldingSpaceImage* const owner_;
  const gfx::ImageSkia placeholder_;
  AsyncBitmapResolver async_bitmap_resolver_;
  std::map<float, gfx::ImageSkia> cache_;

  base::WeakPtrFactory<ImageSkiaSource> weak_factory_{this};
};

// HoldingSpaceImage -----------------------------------------------------------

HoldingSpaceImage::HoldingSpaceImage(const gfx::ImageSkia& placeholder,
                                     AsyncBitmapResolver async_bitmap_resolver)
    : image_skia_(std::make_unique<ImageSkiaSource>(/*owner=*/this,
                                                    placeholder,
                                                    async_bitmap_resolver),
                  placeholder.size()) {}

HoldingSpaceImage::~HoldingSpaceImage() = default;

bool HoldingSpaceImage::operator==(const HoldingSpaceImage& rhs) const {
  return gfx::BitmapsAreEqual(*image_skia_.bitmap(), *rhs.image_skia_.bitmap());
}

std::unique_ptr<HoldingSpaceImage::Subscription>
HoldingSpaceImage::AddImageSkiaChangedCallback(
    CallbackList::CallbackType callback) const {
  return callback_list_.Add(std::move(callback));
}

void HoldingSpaceImage::NotifyUpdated(float scale) {
  // Force invalidate `image_skia_` for `scale` so that it will request the
  // updated `gfx::ImageSkiaRep` at next access.
  image_skia_.RemoveRepresentation(scale);
  image_skia_.RemoveUnsupportedRepresentationsForScale(scale);
  callback_list_.Notify();
}

}  // namespace ash
