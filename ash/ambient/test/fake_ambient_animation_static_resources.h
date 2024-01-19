// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_FAKE_AMBIENT_ANIMATION_STATIC_RESOURCES_H_
#define ASH_AMBIENT_TEST_FAKE_AMBIENT_ANIMATION_STATIC_RESOURCES_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"

namespace cc {
class SkottieWrapper;
}  // namespace cc

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ash {

class ASH_EXPORT FakeAmbientAnimationStaticResources
    : public AmbientAnimationStaticResources {
 public:
  FakeAmbientAnimationStaticResources();
  FakeAmbientAnimationStaticResources(
      const FakeAmbientAnimationStaticResources&) = delete;
  FakeAmbientAnimationStaticResources& operator=(
      const FakeAmbientAnimationStaticResources&) = delete;
  ~FakeAmbientAnimationStaticResources() override;

  // Sets the output for all future calls to GetSkottieWrapper(). If not set,
  // GetSkottieWrapper() will crash with a fatal error.
  void SetSkottieWrapper(scoped_refptr<cc::SkottieWrapper> animation);

  // Sets the |image| that will be returned in future calls to
  // GetStaticImageAsset(asset_id). If the image is not set for an asset,
  // GetStaticImageAsset() will return a null image.
  void SetStaticImageAsset(std::string_view asset_id, gfx::ImageSkia image);

  void set_ui_settings(AmbientUiSettings ui_settings) {
    ui_settings_ = std::move(ui_settings);
  }

  // AmbientAnimationStaticResources implementation:
  const scoped_refptr<cc::SkottieWrapper>& GetSkottieWrapper() const override;
  gfx::ImageSkia GetStaticImageAsset(std::string_view asset_id) const override;
  const AmbientUiSettings& GetUiSettings() const override;

 private:
  scoped_refptr<cc::SkottieWrapper> animation_;
  base::flat_map</*asset_id*/ std::string, gfx::ImageSkia> images_;
  AmbientUiSettings ui_settings_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_FAKE_AMBIENT_ANIMATION_STATIC_RESOURCES_H_
