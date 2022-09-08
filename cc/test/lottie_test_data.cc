// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/lottie_test_data.h"

#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "cc/test/skia_common.h"

namespace cc {
namespace {

std::string ReplaceNamesInAnimation(
    base::StringPiece animation_json,
    const base::flat_map<base::StringPiece, base::StringPiece>& replacements) {
  std::string output(animation_json);
  for (auto [from, to] : replacements) {
    CHECK(!from.empty());
    CHECK(!to.empty());
    base::ReplaceSubstringsAfterOffset(&output, /*start_offset=*/0, from, to);
  }
  return output;
}

}  // namespace

std::string CreateCustomLottieDataWith2ColorNodes(
    base::StringPiece color_node_1,
    base::StringPiece color_node_2) {
  return ReplaceNamesInAnimation(
      kLottieDataWithoutAssets1,
      {{kLottieDataWithoutAssets1Color1Node, color_node_1},
       {kLottieDataWithoutAssets1Color2Node, color_node_2}});
}

std::string CreateCustomLottieDataWith2Assets(
    base::StringPiece custom_asset_id_0,
    base::StringPiece custom_asset_id_1) {
  return ReplaceNamesInAnimation(
      kLottieDataWith2Assets,
      {{"image_0", custom_asset_id_0}, {"image_1", custom_asset_id_1}});
}

std::string CreateCustomLottieDataWith2TextNodes(
    base::StringPiece custom_text_node_name_0,
    base::StringPiece custom_text_node_name_1) {
  return ReplaceNamesInAnimation(
      LoadSkottieFileFromTestData(kLottieDataWith2TextFileName),
      {{kLottieDataWith2TextNode1, custom_text_node_name_0},
       {kLottieDataWith2TextNode2, custom_text_node_name_1}});
}

}  // namespace cc
