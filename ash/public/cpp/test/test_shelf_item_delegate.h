// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_SHELF_ITEM_DELEGATE_H_
#define ASH_PUBLIC_CPP_TEST_TEST_SHELF_ITEM_DELEGATE_H_

#include "ash/public/cpp/shelf_item_delegate.h"

namespace ash {

// A test version of ShelfItemDelegate that does nothing.
class TestShelfItemDelegate : public ShelfItemDelegate {
 public:
  explicit TestShelfItemDelegate(const ShelfID& shelf_id);
  ~TestShelfItemDelegate() override;

  // ShelfItemDelegate:
  void ExecuteCommand(bool from_context_menu,
                      int64_t command_id,
                      int32_t event_flags,
                      int64_t display_id) override;
  void Close() override;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_SHELF_ITEM_DELEGATE_H_
