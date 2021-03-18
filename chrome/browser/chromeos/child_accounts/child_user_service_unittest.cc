// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/child_user_service.h"

#include <memory>
#include <vector>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/child_accounts/child_user_service_factory.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_interface.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limit_utils.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class ChildUserServiceTest : public testing::Test {
 protected:
  ChildUserServiceTest()
      : service_(std::make_unique<ChildUserService>(&profile_)),
        service_test_api_(
            std::make_unique<ChildUserService::TestApi>(service_.get())) {}
  ChildUserServiceTest(const ChildUserServiceTest&) = delete;
  ChildUserServiceTest& operator=(const ChildUserServiceTest&) = delete;
  ~ChildUserServiceTest() override = default;

  // Enables web time limits feature. Recreates ChildUserService object.
  void EnableWebTimeLimits() {
    EnableFeatures({features::kWebTimeLimits});
    service_ = std::make_unique<ChildUserService>(&profile_);
    service_test_api_ =
        std::make_unique<ChildUserService::TestApi>(service_.get());

    // Install Chrome browser and set its app limit.
    app_time::AppActivityRegistry* registry =
        service_test_api_->app_time_controller()->app_registry();
    registry->OnAppInstalled(app_time::GetChromeAppId());
    registry->OnAppAvailable(app_time::GetChromeAppId());
    registry->SetAppLimit(
        app_time::GetChromeAppId(),
        app_time::AppLimit(app_time::AppRestriction::kTimeLimit,
                           base::TimeDelta::FromHours(1), base::Time::Now()));
  }

  Profile* profile() { return &profile_; }

  ChildUserService* service() { return service_.get(); }

  ChildUserService::TestApi* service_test_api() {
    return service_test_api_.get();
  }

 private:
  // Enables given |features|. Recreates ChildUserService object.
  void EnableFeatures(const std::vector<base::Feature>& features) {
    scoped_feature_list_.InitWithFeatures(features, {});
    service_ = std::make_unique<ChildUserService>(&profile_);
    service_test_api_ =
        std::make_unique<ChildUserService::TestApi>(service_.get());
  }

  content::BrowserTaskEnvironment task_environment_;

  TestingProfile profile_;

  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<ChildUserService> service_;
  std::unique_ptr<ChildUserService::TestApi> service_test_api_;
};

// Tests Per-App Time Limits feature.
using PerAppTimeLimitsTest = ChildUserServiceTest;

TEST_F(PerAppTimeLimitsTest, GetAppTimeLimitInterface) {
  EXPECT_EQ(ChildUserServiceFactory::GetForBrowserContext(profile()),
            app_time::AppTimeLimitInterface::Get(profile()));
}

TEST_F(PerAppTimeLimitsTest, PauseAndResumeWebActivity) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  const std::string app_id = extension_misc::kChromeAppId;
  service()->PauseWebActivity(app_id);
  EXPECT_TRUE(service()->WebTimeLimitReached());

  service()->ResumeWebActivity(app_id);
  EXPECT_FALSE(service()->WebTimeLimitReached());
  EXPECT_EQ(base::TimeDelta(), service()->GetWebTimeLimit());
}

TEST_F(PerAppTimeLimitsTest, PauseWebActivityTwice) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  const std::string app_id = extension_misc::kChromeAppId;
  service()->PauseWebActivity(app_id);
  EXPECT_TRUE(service()->WebTimeLimitReached());

  service()->PauseWebActivity(app_id);
  EXPECT_TRUE(service()->WebTimeLimitReached());
}

TEST_F(PerAppTimeLimitsTest, ResumeWebActivityTwice) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  const std::string app_id = extension_misc::kChromeAppId;
  service()->ResumeWebActivity(app_id);

  EXPECT_FALSE(service()->WebTimeLimitReached());
  EXPECT_EQ(base::TimeDelta(), service()->GetWebTimeLimit());

  service()->ResumeWebActivity(app_id);

  EXPECT_FALSE(service()->WebTimeLimitReached());
  EXPECT_EQ(base::TimeDelta(), service()->GetWebTimeLimit());
}

TEST_F(PerAppTimeLimitsTest, WebAppsDontTriggerPauseOrResumeWebActivity) {
  EnableWebTimeLimits();
  EXPECT_FALSE(service()->WebTimeLimitReached());

  const std::string chrome_app_id = extension_misc::kChromeAppId;
  service()->PauseWebActivity(chrome_app_id);

  EXPECT_TRUE(service()->WebTimeLimitReached());

  const std::string web_app_id = "iniodglblcgmngkgdipeiclkdjjpnlbn";
  service()->ResumeWebActivity(web_app_id);
  EXPECT_TRUE(service()->WebTimeLimitReached());
}

}  // namespace chromeos
