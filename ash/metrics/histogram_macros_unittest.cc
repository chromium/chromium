// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/histogram_macros.h"

#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/test/metrics/histogram_tester.h"

namespace ash {

using HistogramMacrosTest = AshTestBase;

TEST_F(HistogramMacrosTest, Basic) {
  base::HistogramTester histograms;
  const char* kTablet = "Test.Tablet";
  const char* kClamshell = "Test.Clamshell";

  UMA_HISTOGRAM_PERCENTAGE_IN_TABLET(kTablet, 1);
  UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(kClamshell, 1);

  histograms.ExpectTotalCount(kClamshell, 1);
  histograms.ExpectTotalCount(kTablet, 0);

  TabletModeControllerTestApi().EnterTabletMode();

  UMA_HISTOGRAM_PERCENTAGE_IN_TABLET(kTablet, 1);
  UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(kClamshell, 1);

  histograms.ExpectTotalCount(kClamshell, 1);
  histograms.ExpectTotalCount(kTablet, 1);
}

}  // namespace ash
