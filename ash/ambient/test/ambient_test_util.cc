// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_test_util.h"

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "cc/paint/skottie_resource_metadata.h"

namespace ash {

std::string GenerateTestLottieDynamicAssetId(int unique_id) {
  return base::StrCat(
      {kLottieDynamicAssetIdPrefix, base::NumberToString(unique_id)});
}

AmbientPhotoConfig GenerateAnimationConfigWithNAssets(int num_assets) {
  cc::SkottieResourceMetadataMap resource_metadata;
  for (int i = 0; i < num_assets; ++i) {
    CHECK(resource_metadata.RegisterAsset(
        "test-resource-path", "test-resource-name",
        GenerateTestLottieDynamicAssetId(/*unique_id=*/i)));
  }
  return CreateAmbientAnimationPhotoConfig(resource_metadata);
}

}  // namespace ash
