// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/test/ambient_test_util.h"

#include "ash/ambient/ambient_constants.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"

namespace ash {

std::string GenerateTestLottieDynamicAssetId(int unique_id) {
  return base::StrCat(
      {kLottieDynamicAssetIdPrefix, base::NumberToString(unique_id)});
}

}  // namespace ash
