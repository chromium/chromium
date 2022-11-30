// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/recipes/time_format_util.h"

#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TimeFormatUtilTest : public testing::Test {
 public:
  TimeFormatUtilTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TimeFormatUtilTest, GetViewedItemText) {
  const auto now = base::Time::Now();
  EXPECT_EQ("Viewed today", GetViewedItemText(now));
  EXPECT_EQ("Viewed yesterday", GetViewedItemText(now - base::Days(1)));
  EXPECT_EQ("Viewed in the past week", GetViewedItemText(now - base::Days(2)));
  EXPECT_EQ("Viewed in the past month",
            GetViewedItemText(now - base::Days(15)));
  EXPECT_EQ("Viewed previously", GetViewedItemText(now - base::Days(100)));
}
