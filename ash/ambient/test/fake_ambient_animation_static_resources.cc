// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/fake_ambient_animation_static_resources.h"

#include <string_view>
#include <utility>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/notreached.h"
#include "cc/paint/skottie_wrapper.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

FakeAmbientAnimationStaticResources::FakeAmbientAnimationStaticResources()
    : ui_settings_(personalization_app::mojom::AmbientTheme::kFeelTheBreeze) {}

FakeAmbientAnimationStaticResources::~FakeAmbientAnimationStaticResources() =
    default;

void FakeAmbientAnimationStaticResources::SetSkottieWrapper(
    scoped_refptr<cc::SkottieWrapper> animation) {
  CHECK(animation);
  CHECK(animation->is_valid());
  animation_ = std::move(animation);
}

void FakeAmbientAnimationStaticResources::SetStaticImageAsset(
    std::string_view asset_id,
    gfx::ImageSkia image) {
  images_[std::string(asset_id)] = std::move(image);
}

const scoped_refptr<cc::SkottieWrapper>&
FakeAmbientAnimationStaticResources::GetSkottieWrapper() const {
  CHECK(animation_);
  return animation_;
}

gfx::ImageSkia FakeAmbientAnimationStaticResources::GetStaticImageAsset(
    std::string_view asset_id) const {
  auto iter = images_.find(std::string(asset_id));
  return iter == images_.end() ? gfx::ImageSkia() : iter->second;
}

const AmbientUiSettings& FakeAmbientAnimationStaticResources::GetUiSettings()
    const {
  return ui_settings_;
}

}  // namespace ash
