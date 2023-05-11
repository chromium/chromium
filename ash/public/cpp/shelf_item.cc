// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_item.h"

namespace ash {

ShelfItem::ShelfItem() = default;
ShelfItem::ShelfItem(const ShelfItem& shelf_item) = default;
ShelfItem::~ShelfItem() = default;

bool ShelfItem::IsPinStateForced() const {
  return pinned_by_policy || pin_state_forced_by_type;
}

}  // namespace ash
