// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_TEST_API_IMPL_H_
#define ASH_SHELF_SHELF_TEST_API_IMPL_H_

#include "ash/public/cpp/shelf_test_api.h"
#include "base/macros.h"

namespace ash {

// Allows tests to access private state of the shelf.
class ShelfTestApiImpl : public ShelfTestApi {
 public:
  ShelfTestApiImpl();
  ~ShelfTestApiImpl() override;

  // ShelfTestApi:
  bool IsVisible() override;
  bool IsAlignmentBottomLocked() override;
  views::View* GetHomeButton() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfTestApiImpl);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_TEST_API_IMPL_H_
