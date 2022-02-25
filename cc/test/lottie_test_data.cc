// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/lottie_test_data.h"

#include "base/strings/string_util.h"
#include "cc/test/skia_common.h"

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

std::string CreateCustomLottieDataWith2TextNodes(
    base::StringPiece custom_text_node_name_0,
    base::StringPiece custom_text_node_name_1) {
  CHECK(!custom_text_node_name_0.empty());
  CHECK(!custom_text_node_name_1.empty());
  std::string output =
      LoadSkottieFileFromTestData(kLottieDataWith2TextFileName);
  base::ReplaceSubstringsAfterOffset(&output, /*start_offset=*/0,
                                     kLottieDataWith2TextNode1,
                                     custom_text_node_name_0);
  base::ReplaceSubstringsAfterOffset(&output, /*start_offset=*/0,
                                     kLottieDataWith2TextNode2,
                                     custom_text_node_name_1);
  return output;
}

}  // namespace cc
