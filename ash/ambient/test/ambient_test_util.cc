// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_test_util.h"

#include <optional>
#include <string_view>

#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace ash {

std::string GenerateLottieCustomizableIdForTesting(int unique_id) {
  return base::StrCat(
      {kLottieCustomizableIdPrefix, base::NumberToString(unique_id)});
}

std::string GenerateLottieDynamicAssetIdForTesting(std::string_view position,
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
        /*size=*/std::nullopt));
  }
  return CreateAmbientAnimationPhotoConfig(resource_metadata);
}

}  // namespace ash
