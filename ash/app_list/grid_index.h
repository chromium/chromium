// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_GRID_INDEX_H_
#define ASH_APP_LIST_GRID_INDEX_H_

#include <string>
#include <tuple>

#include "ash/ash_export.h"

namespace ash {

// Represents the index to an item view in the grid.
struct ASH_EXPORT GridIndex {
  GridIndex() = default;
  GridIndex(int page, int slot);

  bool operator==(const GridIndex& other) const;
  bool operator!=(const GridIndex& other) const;
  bool operator<(const GridIndex& other) const;

  // Whether the grid index is a valid index, i.e. whether page and slot are
  // non-negative. This method does *not* check whether the index exists in an
  // apps grid.
  bool IsValid() const;

  std::string ToString() const;

  // Which page an item view is on.
  int page = -1;

  // Which slot in the page an item view is in.
  int slot = -1;
};

}  // namespace ash

#endif  // ASH_APP_LIST_GRID_INDEX_H_
