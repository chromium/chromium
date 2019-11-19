// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_MULTI_DISPLAY_OVERVIEW_AND_SPLIT_VIEW_TEST_H_
#define ASH_WM_SPLITVIEW_MULTI_DISPLAY_OVERVIEW_AND_SPLIT_VIEW_TEST_H_

#include "ash/ash_export.h"
#include "ash/test/ash_test_base.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"

namespace ash {

// Test with a parameter that determines whether to enable the
// |ash::features::kMultiDisplayOverviewAndSplitView| feature flag, to ensure
// that old overview and split view functionality still works.
class ASH_EXPORT MultiDisplayOverviewAndSplitViewTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  MultiDisplayOverviewAndSplitViewTest();
  ~MultiDisplayOverviewAndSplitViewTest() override;

  // AshTestBase:
  void SetUp() override;
  void TearDown() override;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiDisplayOverviewAndSplitViewTest);
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_MULTI_DISPLAY_OVERVIEW_AND_SPLIT_VIEW_TEST_H_
