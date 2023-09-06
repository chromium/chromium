// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/ash/fileapi/recent_file.h"
#include "chrome/browser/ash/fileapi/recent_source.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace ash {

namespace {

class RecentSourceTest : public testing::Test {
 public:
  RecentSourceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(RecentSourceTest, NeverIsLate) {
  RecentSource::Params params(nullptr, GURL(u""), 100, base::Time::Max(),
                              base::TimeTicks::Max(),
                              RecentSource::FileType::kAll, base::DoNothing());
  EXPECT_FALSE(params.IsLate());
  task_environment_.FastForwardBy(base::Hours(99));
  EXPECT_FALSE(params.IsLate());
}

TEST_F(RecentSourceTest, IsLate) {
  RecentSource::Params params(nullptr, GURL(u""), 100, base::Time::Max(),
                              base::TimeTicks::Now() + base::Milliseconds(1000),
                              RecentSource::FileType::kAll, base::DoNothing());
  EXPECT_FALSE(params.IsLate());
  task_environment_.FastForwardBy(base::Milliseconds(999));
  EXPECT_FALSE(params.IsLate());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_FALSE(params.IsLate());
  task_environment_.FastForwardBy(base::Milliseconds(1));
  EXPECT_TRUE(params.IsLate());
}

}  // namespace
}  // namespace ash
