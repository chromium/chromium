// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/gdi_debug_util_win.h"

#include <windows.h>

#include "base/win/scoped_hdc.h"
#include "testing/gtest/include/gtest/gtest.h"

// GDI handles can occasionally come out of nowhere on the shared table, so
// when writing the tests below, make sure you do differential snapshots to
// count handles.

TEST(GdiDebugUtilWin, GdiHandleCountsCreateDC) {
  base::debug::GdiHandleCounts handle_counts_start =
      base::debug::GetGDIHandleCountsInCurrentProcessForTesting();
  base::win::ScopedGetDC dc(nullptr);
  ASSERT_TRUE(static_cast<HDC>(dc));
  base::debug::GdiHandleCounts handle_counts_now =
      base::debug::GetGDIHandleCountsInCurrentProcessForTesting();
  EXPECT_EQ(1, handle_counts_now.dcs - handle_counts_start.dcs);
  EXPECT_EQ(
      1, handle_counts_now.total_tracked - handle_counts_start.total_tracked);
}

// TODO(robliao): Create tests for other types once we figure out how often GDI
// updates the handle table.
