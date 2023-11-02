// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_shelf_item_delegate.h"

namespace ash {

TestShelfItemDelegate::TestShelfItemDelegate(const ShelfID& shelf_id)
    : ShelfItemDelegate(shelf_id) {}

TestShelfItemDelegate::~TestShelfItemDelegate() = default;

void TestShelfItemDelegate::ExecuteCommand(bool from_context_menu,
                                           int64_t command_id,
                                           int32_t event_flags,
                                           int64_t display_id) {}

void TestShelfItemDelegate::Close() {}

}  // namespace ash
