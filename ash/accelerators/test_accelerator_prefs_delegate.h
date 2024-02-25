// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_TEST_ACCELERATOR_PREFS_DELEGATE_H_
#define ASH_ACCELERATORS_TEST_ACCELERATOR_PREFS_DELEGATE_H_

#include "ash/accelerators/accelerator_prefs_delegate.h"

namespace ash {

class TestAcceleratorPrefsDelegate : public AcceleratorPrefsDelegate {
 public:
  TestAcceleratorPrefsDelegate() = default;
  TestAcceleratorPrefsDelegate(const TestAcceleratorPrefsDelegate&) = delete;
  TestAcceleratorPrefsDelegate& operator=(const TestAcceleratorPrefsDelegate&) =
      delete;
  ~TestAcceleratorPrefsDelegate() override = default;

  // ash::AcceleratorPrefsDelegate:
  bool IsUserEnterpriseManaged() const override;
};

}  // namespace ash

#endif  // ASH_ACCELERATORS_TEST_ACCELERATOR_PREFS_DELEGATE_H_
