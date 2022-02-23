// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/lottie_test_data.h"

#include "base/strings/string_util.h"

namespace cc {

std::string CreateCustomLottieDataWith2Assets(
    base::StringPiece custom_asset_id_0,
    base::StringPiece custom_asset_id_1) {
  CHECK(!custom_asset_id_0.empty());
  CHECK(!custom_asset_id_1.empty());
  std::string output(kLottieDataWith2Assets);
  base::ReplaceSubstringsAfterOffset(&output, /*start_offset=*/0, "image_0",
                                     custom_asset_id_0);
  base::ReplaceSubstringsAfterOffset(&output, /*start_offset=*/0, "image_1",
                                     custom_asset_id_1);
  return output;
}

}  // namespace cc
