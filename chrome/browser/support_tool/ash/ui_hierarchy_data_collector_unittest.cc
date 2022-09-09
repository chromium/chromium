// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/support_tool/ash/ui_hierarchy_data_collector.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

class UiHierarchyDataCollectorTest : public ::testing::Test {
 public:
  UiHierarchyDataCollectorTest() = default;

  UiHierarchyDataCollectorTest(const UiHierarchyDataCollectorTest&) = delete;
  UiHierarchyDataCollectorTest& operator=(const UiHierarchyDataCollectorTest&) =
      delete;
};

TEST_F(UiHierarchyDataCollectorTest, RemoveWindowTitles) {
  // A test case that contain multiple window titles and other information.
  std::string data =
      "header=ksjdhhs\nsome-other-field=skjdhjskh\ntitle=one window\nkskhs "
      "sjhjksh ksjdhj\ntitle=another window here\nsjdjhj sjdjshs sjjskh\n";
  std::string expected_result =
      "header=ksjdhhs\nsome-other-field=skjdhjskh\ntitle=<REDACTED>\nkskhs "
      "sjhjksh ksjdhj\ntitle=<REDACTED>\nsjdjhj sjdjshs sjjskh\n";

  EXPECT_EQ(UiHierarchyDataCollector::RemoveWindowTitles(data),
            expected_result);

  // A test case that doesn't contain any window title in it.
  data = "ksjdhhs\nskjdhjskh\nkskhs sjhjksh ksjdhj\nsjdjhj sjdjshs sjjskh\n";
  expected_result =
      "ksjdhhs\nskjdhjskh\nkskhs sjhjksh ksjdhj\nsjdjhj sjdjshs sjjskh\n";

  EXPECT_EQ(UiHierarchyDataCollector::RemoveWindowTitles(data),
            expected_result);

  // A test case with only one window title which contains blank space in
  // between.
  data = "title=one window\n";
  expected_result = "title=<REDACTED>\n";

  EXPECT_EQ(UiHierarchyDataCollector::RemoveWindowTitles(data),
            expected_result);

  // A test case with only one window title.
  data = "title=window\n";
  expected_result = "title=<REDACTED>\n";

  EXPECT_EQ(UiHierarchyDataCollector::RemoveWindowTitles(data),
            expected_result);
}
