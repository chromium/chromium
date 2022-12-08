// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/reorder/app_list_reorder_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"

using AppListReorderUtilTest = testing::Test;

namespace app_list {

gfx::ImageSkia GetRedIconWithBackgroundColorOf(SkColor color) {
  const int width = 64;
  const int height = 64;

  SkBitmap icon;
  icon.allocN32Pixels(width, height);
  icon.eraseColor(color);

  icon.erase(gfx::kGoogleRed300,
             {width / 4, height / 4, width / 2, height / 2});

  return gfx::ImageSkia::CreateFrom1xBitmap(icon);
}

TEST_F(AppListReorderUtilTest, SortableIconColorTest) {
  const int red_hue = 49;

  // Test a red icon with a black background color.
  ash::IconColor group = reorder::GetSortableIconColorForApp(
      "test_app1", GetRedIconWithBackgroundColorOf(SK_ColorBLACK));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_BLACK);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a white background color.
  group = reorder::GetSortableIconColorForApp(
      "test_app2", GetRedIconWithBackgroundColorOf(SK_ColorWHITE));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_WHITE);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a red background color.
  group = reorder::GetSortableIconColorForApp(
      "test_app3", GetRedIconWithBackgroundColorOf(SK_ColorRED));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_RED);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a yellow background color.
  group = reorder::GetSortableIconColorForApp(
      "test_app4", GetRedIconWithBackgroundColorOf(SK_ColorYELLOW));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_YELLOW);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a green background color.
  group = reorder::GetSortableIconColorForApp(
      "test_app5", GetRedIconWithBackgroundColorOf(SK_ColorGREEN));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_GREEN);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a blue background color.
  group = reorder::GetSortableIconColorForApp(
      "test_app6", GetRedIconWithBackgroundColorOf(SK_ColorBLUE));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_BLUE);
  EXPECT_EQ(group.hue(), red_hue);

  // Test a red icon with a magenta background color.
  group = reorder::GetSortableIconColorForApp(
      "test_app7", GetRedIconWithBackgroundColorOf(SK_ColorMAGENTA));
  EXPECT_EQ(group.background_color(),
            sync_pb::AppListSpecifics::ColorGroup::
                AppListSpecifics_ColorGroup_COLOR_MAGENTA);
  EXPECT_EQ(group.hue(), red_hue);
}

}  // namespace app_list
