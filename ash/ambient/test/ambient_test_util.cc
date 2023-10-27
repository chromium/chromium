// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_test_util.h"

#include <vector>

#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/test/ash_test_util.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_util.h"

namespace ash {

std::string GenerateLottieCustomizableIdForTesting(int unique_id) {
  return base::StrCat(
      {kLottieCustomizableIdPrefix, base::NumberToString(unique_id)});
}

std::string GenerateLottieDynamicAssetIdForTesting(base::StringPiece position,
                                                   int idx) {
  CHECK(!position.empty());
  return base::StrCat({kLottieCustomizableIdPrefix, "_Photo_Position", position,
                       "_", base::NumberToString(idx)});
}

AmbientPhotoConfig GenerateAnimationConfigWithNAssets(int num_assets) {
  cc::SkottieResourceMetadataMap resource_metadata;
  for (int i = 0; i < num_assets; ++i) {
    CHECK(resource_metadata.RegisterAsset(
        "test-resource-path", "test-resource-name",
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/base::NumberToString(i), /*idx=*/1),
        /*size=*/absl::nullopt));
  }
  return CreateAmbientAnimationPhotoConfig(resource_metadata);
}

std::string CreateEncodedImageForTesting(gfx::Size size,
                                         gfx::ImageSkia* image_out) {
  gfx::ImageSkia test_image = CreateSolidColorTestImage(size, SK_ColorGREEN);
  CHECK(!test_image.isNull());
  if (image_out) {
    *image_out = test_image;
  }
  std::vector<unsigned char> encoded_image;
  CHECK(gfx::JPEG1xEncodedDataFromImage(gfx::Image(test_image), 100,
                                        &encoded_image));
  return std::string(reinterpret_cast<const char*>(encoded_image.data()),
                     encoded_image.size());
}

}  // namespace ash
