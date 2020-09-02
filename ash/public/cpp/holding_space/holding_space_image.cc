// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_image.h"

#include <memory>

#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/skia_util.h"

namespace ash {

// HoldingSpaceImage::ImageSkiaSource ------------------------------------------

class HoldingSpaceImage::ImageSkiaSource : public gfx::ImageSkiaSource {
 public:
  explicit ImageSkiaSource(const gfx::ImageSkia& placeholder)
      : placeholder_(placeholder) {}
  ImageSkiaSource(const ImageSkiaSource&) = delete;
  ImageSkiaSource& operator=(const ImageSkiaSource&) = delete;
  ~ImageSkiaSource() override = default;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    // TODO(dmblack): Retrieve thumbnail and call `NotifyUpdated()` when ready.
    return placeholder_.GetRepresentation(scale);
  }

  const gfx::ImageSkia placeholder_;
};

// HoldingSpaceImage -----------------------------------------------------------

HoldingSpaceImage::HoldingSpaceImage(const gfx::ImageSkia& placeholder)
    : image_skia_(std::make_unique<ImageSkiaSource>(placeholder),
                  placeholder.size()) {}

HoldingSpaceImage::~HoldingSpaceImage() {
  NotifyDestroying();
}

bool HoldingSpaceImage::operator==(const HoldingSpaceImage& rhs) const {
  return gfx::BitmapsAreEqual(*image_skia_.bitmap(), *rhs.image_skia_.bitmap());
}

void HoldingSpaceImage::AddObserver(Observer* observer) const {
  observers_.AddObserver(observer);
}

void HoldingSpaceImage::RemoveObserver(Observer* observer) const {
  observers_.RemoveObserver(observer);
}

void HoldingSpaceImage::NotifyDestroying() {
  for (auto& observer : observers_)
    observer.OnHoldingSpaceImageDestroying(this);
}

void HoldingSpaceImage::NotifyUpdated(float scale) {
  // Force invalidate `image_skia_` for `scale` so that it will request the
  // updated `gfx::ImageSkiaRep` at next access.
  image_skia_.RemoveRepresentation(scale);
  image_skia_.RemoveUnsupportedRepresentationsForScale(scale);

  for (auto& observer : observers_)
    observer.OnHoldingSpaceImageUpdated(this);
}

}  // namespace ash
