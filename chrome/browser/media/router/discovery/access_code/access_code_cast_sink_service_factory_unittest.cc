// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_sink_service_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_test_util.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media_router {

class AccessCodeCastSinkServiceFactoryTest : public testing::Test {
 protected:
  AccessCodeCastSinkServiceFactoryTest() = default;
  ~AccessCodeCastSinkServiceFactoryTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeatures({},
                                   {features::kAccessCodeCastRememberDevices});
    TestingProfile::Builder profile_builder;
    auto pref_service =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(pref_service->registry());
    profile_builder.SetPrefService(std::move(pref_service));
    profile_ = profile_builder.Build();
    AccessCodeCastSinkServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&MockAccessCodeCastSinkService::Create));
    MediaRouterFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&MockMediaRouter::Create));
  }

  void TearDown() override { profile_.reset(); }

  Profile* profile() { return profile_.get(); }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(AccessCodeCastSinkServiceFactoryTest, PrefDisabledReturnsNullPtr) {
  profile()->GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, false);
  ASSERT_FALSE(AccessCodeCastSinkServiceFactory::GetForProfile(profile()));
}

TEST_F(AccessCodeCastSinkServiceFactoryTest, PrefEnabledReturnsValidService) {
  profile()->GetPrefs()->SetBoolean(prefs::kAccessCodeCastEnabled, true);
  ASSERT_TRUE(AccessCodeCastSinkServiceFactory::GetForProfile(profile()));
}

}  // namespace media_router
