// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/task_module/time_format_util.h"

#include "base/time/time.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class TimeFormatUtilTest : public testing::Test {
 public:
  TimeFormatUtilTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  int AsTimestamp(const base::Time& time) {
    return (time - base::Time::UnixEpoch()).InSeconds();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(TimeFormatUtilTest, GetViewedItemText) {
  const auto now = base::Time::Now();
  EXPECT_EQ("Viewed today", GetViewedItemText(AsTimestamp(now)));
  EXPECT_EQ("Viewed yesterday",
            GetViewedItemText(AsTimestamp(now - base::Days(1))));
  EXPECT_EQ("Viewed in the past week",
            GetViewedItemText(AsTimestamp(now - base::Days(2))));
  EXPECT_EQ("Viewed in the past month",
            GetViewedItemText(AsTimestamp(now - base::Days(30))));
  EXPECT_EQ("Viewed previously",
            GetViewedItemText(AsTimestamp(now - base::Days(100))));
}
