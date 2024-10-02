// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/saved_tab_groups/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

class TabGroupSyncServiceFactoryTest : public testing::Test {
 protected:
  TabGroupSyncServiceFactoryTest() = default;

  ~TabGroupSyncServiceFactoryTest() override = default;

  void InitService(bool enable_feature) {
    profile_ = TestingProfile::Builder().Build();

    base::flat_map<base::test::FeatureRef, bool> feature_states;
#if BUILDFLAG(IS_ANDROID)
    feature_states.try_emplace(tab_groups::kTabGroupSyncAndroid,
                               enable_feature);
#else
    feature_states.try_emplace(tab_groups::kTabGroupSyncServiceDesktopMigration,
                               enable_feature);
#endif

    scoped_feature_list_.InitWithFeatureStates(feature_states);
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TabGroupSyncServiceFactoryTest, FeatureDisabledReturnsNullService) {
  InitService(/*enable_feature=*/false);
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(profile_.get());
  EXPECT_FALSE(service);
}

TEST_F(TabGroupSyncServiceFactoryTest, ServiceCreatedInRegularProfile) {
  InitService(/*enable_feature=*/true);
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service);
}

TEST_F(TabGroupSyncServiceFactoryTest, ServiceNotCreatedInIncognito) {
  InitService(/*enable_feature=*/true);
  Profile* otr_profile = profile_.get()->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_FALSE(TabGroupSyncServiceFactory::GetForProfile(otr_profile));
}

}  // namespace
}  // namespace tab_groups
