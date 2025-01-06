// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/hardware_check.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base::win {

TEST(IsWin11UpgradeEligible, ExpectNoCrash) {
  // It's not worthwhile to check the validity of the return value
  // so just check for crashes.
  IsWin11UpgradeEligible();
}

}  // namespace base::win
