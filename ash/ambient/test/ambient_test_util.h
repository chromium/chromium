// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_
#define ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_

#include <string>
#include <string_view>

namespace ash {

struct AmbientPhotoConfig;

// Generates a generic customizable lottie id that incorporates the |unique_id|
// in it.
std::string GenerateLottieCustomizableIdForTesting(int unique_id);

// Generates a lottie dynamic image asset id for testing purposes (see
// ParseDynamicLottieAssetId() for details).
std::string GenerateLottieDynamicAssetIdForTesting(std::string_view position,
                                                   int idx);

// Returns an AmbientPhotoConfig for a lottie animation with the number of
// assets specified by |num_assets|,
AmbientPhotoConfig GenerateAnimationConfigWithNAssets(int num_assets);

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_
