// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/fake_ambient_animation_static_resources.h"

#include <utility>

#include "base/notreached.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

FakeAmbientAnimationStaticResources::FakeAmbientAnimationStaticResources() =
    default;

FakeAmbientAnimationStaticResources::~FakeAmbientAnimationStaticResources() =
    default;

void FakeAmbientAnimationStaticResources::SetLottieData(
    std::string lottie_data) {
  lottie_data_ = std::move(lottie_data);
}

void FakeAmbientAnimationStaticResources::SetStaticImageAsset(
    base::StringPiece asset_id,
    gfx::ImageSkia image) {
  images_[std::string(asset_id)] = std::move(image);
}

base::StringPiece FakeAmbientAnimationStaticResources::GetLottieData() const {
  return lottie_data_;
}

gfx::ImageSkia FakeAmbientAnimationStaticResources::GetStaticImageAsset(
    base::StringPiece asset_id) const {
  auto iter = images_.find(std::string(asset_id));
  return iter == images_.end() ? gfx::ImageSkia() : iter->second;
}

}  // namespace ash
