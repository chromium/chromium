// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/grid_index.h"

#include <sstream>

namespace ash {

GridIndex::GridIndex(int page, int slot) : page(page), slot(slot) {}

bool GridIndex::operator==(const GridIndex& other) const {
  return page == other.page && slot == other.slot;
}

bool GridIndex::operator!=(const GridIndex& other) const {
  return page != other.page || slot != other.slot;
}

bool GridIndex::operator<(const GridIndex& other) const {
  return std::tie(page, slot) < std::tie(other.page, other.slot);
}

bool GridIndex::IsValid() const {
  return page >= 0 && slot >= 0;
}

std::string GridIndex::ToString() const {
  std::stringstream ss;
  ss << "Page: " << page << ", Slot: " << slot;
  return ss.str();
}

}  // namespace ash
