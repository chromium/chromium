// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#ifndef ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_
#define ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_

#include "base/strings/string_piece.h"

namespace gfx {
class ImageSkia;
class Size;
}  // namespace gfx

namespace ash {

struct AmbientPhotoConfig;

// Generates a generic customizable lottie id that incorporates the |unique_id|
// in it.
std::string GenerateLottieCustomizableIdForTesting(int unique_id);

// Generates a lottie dynamic image asset id for testing purposes (see
// ParseDynamicLottieAssetId() for details).
std::string GenerateLottieDynamicAssetIdForTesting(base::StringPiece position,
                                                   int idx);

// Returns an AmbientPhotoConfig for a lottie animation with the number of
// assets specified by |num_assets|,
AmbientPhotoConfig GenerateAnimationConfigWithNAssets(int num_assets);

// Creates a solid color (hard-coded) image with the given `size`, and returns
// its encoded representation. `image_out` is filled with the raw decoded image
// if provided.
//
// This function can never fail.
std::string CreateEncodedImageForTesting(gfx::Size size,
                                         gfx::ImageSkia* image_out = nullptr);

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_
