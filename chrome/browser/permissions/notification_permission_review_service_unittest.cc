// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/notification_permission_review_service_factory.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace site_settings {

class ReviewNotificationPermissionsHelperTest : public testing::Test {
 protected:
  TestingProfile* profile() { return &profile_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
};

TEST_F(ReviewNotificationPermissionsHelperTest,
       CheckReviewNotificationPermissions) {
  auto* service =
      NotificationPermissionsReviewServiceFactory::GetForProfile(profile());
  auto notification_permissions = service->GetNotificationSiteListForReview();
  { EXPECT_EQ(0UL, notification_permissions.size()); }
}

}  // namespace site_settings
