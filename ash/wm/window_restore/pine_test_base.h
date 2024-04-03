// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_TEST_BASE_H_
#define ASH_WM_WINDOW_RESTORE_PINE_TEST_BASE_H_

#include "ash/test/ash_test_base.h"

namespace ash {

class PineTestBase : public AshTestBase {
 public:
  PineTestBase();
  PineTestBase(const PineTestBase&) = delete;
  PineTestBase& operator=(const PineTestBase&) = delete;
  ~PineTestBase() override;

  PrefService* GetTestPrefService();

  // AshTestBase:
  void SetUp() override;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_TEST_BASE_H_
