// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_FAKE_AMBIENT_ANIMATION_STATIC_RESOURCES_H_
#define ASH_AMBIENT_TEST_FAKE_AMBIENT_ANIMATION_STATIC_RESOURCES_H_

#include <memory>
#include <string>

#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_piece.h"

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

  // Sets the output for all future calls to GetLottieData(). If not set,
  // GetLottieData() will return an empty string.
  void SetLottieData(std::string lottie_data);

  // Sets the |image| that will be returned in future calls to
  // GetStaticImageAsset(asset_id). If the image is not set for an asset,
  // GetStaticImageAsset() will return a null image.
  void SetStaticImageAsset(base::StringPiece asset_id, gfx::ImageSkia image);

  // AmbientAnimationStaticResources implementation:
  base::StringPiece GetLottieData() const override;
  gfx::ImageSkia GetStaticImageAsset(base::StringPiece asset_id) const override;

 private:
  std::string lottie_data_;
  base::flat_map</*asset_id*/ std::string, gfx::ImageSkia> images_;
};

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_FAKE_AMBIENT_ANIMATION_STATIC_RESOURCES_H_
