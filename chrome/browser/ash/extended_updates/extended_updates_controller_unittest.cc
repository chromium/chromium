// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

TEST(ExtendedUpdatesControllerTest, ShouldRequestExtendedUpdatesOptIn) {
  EXPECT_FALSE(ShouldRequestExtendedUpdatesOptIn());
}

}  // namespace ash
