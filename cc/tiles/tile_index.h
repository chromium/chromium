// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_TILE_INDEX_H_
#define CC_TILES_TILE_INDEX_H_

#include <tuple>
#include <utility>

#include "cc/cc_export.h"

namespace cc {

// An index identifying a specific tile within some tiling. A tiling is a 2D
// grid of non-overlapping, tightly packed, uniformly-sized tiles.
struct CC_EXPORT TileIndex {
  TileIndex() = default;
  TileIndex(int i, int j) : i(i), j(j) {}

  explicit TileIndex(const std::pair<int, int>& index)
      : i(index.first), j(index.second) {}

  bool operator==(const TileIndex& other) const {
    return std::tie(i, j) == std::tie(other.i, other.j);
  }

  bool operator!=(const TileIndex& other) const {
    return std::tie(i, j) != std::tie(other.i, other.j);
  }

  bool operator<(const TileIndex& other) const {
    return std::tie(i, j) < std::tie(other.i, other.j);
  }

  // Zero-based column index of the tile.
  int i = 0;

  // Zero-based row index of the tile.
  int j = 0;
};

}  // namespace cc

template <>
struct CC_EXPORT std::hash<cc::TileIndex> {
  size_t operator()(const cc::TileIndex& index) const noexcept {
    uint16_t value1 = static_cast<uint16_t>(index.i);
    uint16_t value2 = static_cast<uint16_t>(index.j);
    uint32_t value1_32 = value1;
    return (value1_32 << 16) | value2;
  }
};

#endif  // CC_TILES_TILE_INDEX_H_
