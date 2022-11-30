// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_user_service.h"

#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class ChildUserServiceTest : public testing::Test {
 protected:
  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

// Tests Per-App Time Limits feature.
using PerAppTimeLimitsTest = ChildUserServiceTest;

TEST_F(PerAppTimeLimitsTest, GetAppTimeLimitInterface) {
  EXPECT_EQ(ChildUserServiceFactory::GetForBrowserContext(profile()),
            app_time::AppTimeLimitInterface::Get(profile()));
}

}  // namespace ash
