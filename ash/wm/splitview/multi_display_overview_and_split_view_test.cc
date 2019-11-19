// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/multi_display_overview_and_split_view_test.h"

#include "ash/public/cpp/ash_features.h"

namespace ash {

MultiDisplayOverviewAndSplitViewTest::MultiDisplayOverviewAndSplitViewTest() =
    default;

MultiDisplayOverviewAndSplitViewTest::~MultiDisplayOverviewAndSplitViewTest() =
    default;

void MultiDisplayOverviewAndSplitViewTest::SetUp() {
  if (GetParam()) {
    scoped_feature_list_.InitAndEnableFeature(
        ash::features::kMultiDisplayOverviewAndSplitView);
  }
  AshTestBase::SetUp();
}

void MultiDisplayOverviewAndSplitViewTest::TearDown() {
  AshTestBase::TearDown();
  scoped_feature_list_.Reset();
}

}  // namespace ash
