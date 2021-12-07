// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#ifndef ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_
#define ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_

namespace ash {

// Generates a dynamic asset id (see IsDynamicLottieAsset() in ambient_util.h)
// that incorporates the |unique_id| in it.
std::string GenerateTestLottieDynamicAssetId(int unique_id);

}  // namespace ash

#endif  // ASH_AMBIENT_TEST_AMBIENT_TEST_UTIL_H_
