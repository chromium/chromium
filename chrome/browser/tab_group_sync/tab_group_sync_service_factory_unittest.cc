// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"

#include <memory>

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

  void InitService() { profile_ = TestingProfile::Builder().Build(); }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(TabGroupSyncServiceFactoryTest, ServiceCreatedInRegularProfile) {
  InitService();
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(profile_.get());
  EXPECT_TRUE(service);
}

TEST_F(TabGroupSyncServiceFactoryTest, ServiceNotCreatedInIncognito) {
  InitService();
  Profile* otr_profile = profile_.get()->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID(), /*create_if_needed=*/true);
  EXPECT_FALSE(TabGroupSyncServiceFactory::GetForProfile(otr_profile));
}

}  // namespace
}  // namespace tab_groups
