// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_feature_tour.h"

#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using PickerFeatureTourTest = AshTestBase;

TEST_F(PickerFeatureTourTest, ShowShowsDialog) {
  PickerFeatureTour feature_tour;

  feature_tour.Show();

  EXPECT_NE(feature_tour.widget_for_testing(), nullptr);
}

}  // namespace
}  // namespace ash
