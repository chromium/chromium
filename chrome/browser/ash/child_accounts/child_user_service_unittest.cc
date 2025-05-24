// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/child_user_service.h"

#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using testing::NotNull;

class ChildUserServiceTest : public testing::Test {
 protected:
  Profile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ChildUserServiceTest, GetForBrowserContext) {
  EXPECT_THAT(ChildUserServiceFactory::GetForBrowserContext(profile()),
              NotNull());
}

}  // namespace ash
