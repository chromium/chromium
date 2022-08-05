// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service_factory.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ElementsAre;

namespace apps::deduplication {

class AppDeduplicationServiceTest : public testing::Test {
 protected:
  AppDeduplicationServiceTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAppDeduplicationService);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppDeduplicationServiceTest, ServiceAccessPerProfile) {
  TestingProfile::Builder profile_builder;

  // We expect an App Deduplication Service in a regular profile.
  auto profile = profile_builder.Build();
  EXPECT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  // We expect App Deduplication Service to be unsupported in incognito.
  TestingProfile::Builder incognito_builder;
  auto* incognito_profile = incognito_builder.BuildIncognito(profile.get());
  EXPECT_FALSE(
      AppDeduplicationServiceFactory::
          IsAppDeduplicationServiceAvailableForProfile(incognito_profile));
  EXPECT_EQ(nullptr,
            AppDeduplicationServiceFactory::GetForProfile(incognito_profile));

  // We expect a different App Deduplication Service in the Guest Session
  // profile.
  TestingProfile::Builder guest_builder;
  guest_builder.SetGuestSession();
  auto guest_profile = guest_builder.Build();

  // App Deduplication Service is not available for original profile.
  EXPECT_FALSE(
      AppDeduplicationServiceFactory::
          IsAppDeduplicationServiceAvailableForProfile(guest_profile.get()));
  EXPECT_EQ(nullptr,
            AppDeduplicationServiceFactory::GetForProfile(guest_profile.get()));

  // App Deduplication Service is available for OTR profile in Guest mode.
  auto* guest_otr_profile =
      guest_profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_TRUE(
      AppDeduplicationServiceFactory::
          IsAppDeduplicationServiceAvailableForProfile(guest_otr_profile));
  auto* guest_otr_service =
      AppDeduplicationServiceFactory::GetForProfile(guest_otr_profile);
  EXPECT_NE(nullptr, guest_otr_service);
  EXPECT_NE(guest_otr_service, service);
}

TEST_F(AppDeduplicationServiceTest, OnDuplicatedAppsMapUpdated) {
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  std::string binary_pb = "";
  base::FilePath install_dir("/");
  apps::AppProvisioningDataManager::Get()->PopulateFromDynamicUpdate(
      binary_pb, install_dir);

  std::string test_key = "test_key";
  std::string arc_app_id = "test_arc_app_id";
  auto it =
      service->entry_to_group_map_.find(EntryId(arc_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(test_key, it->second);

  std::string web_app_id = "test_web_app_id";
  it = service->entry_to_group_map_.find(EntryId(web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(test_key, it->second);

  auto map_it = service->duplication_map_.find(test_key);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(arc_app_id, AppType::kArc)),
                          Entry(EntryId(web_app_id, AppType::kWeb))));
}

// Test exact match entry ids.
TEST_F(AppDeduplicationServiceTest, ExactDuplicate) {
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  std::string binary_pb = "";
  base::FilePath install_dir("/");
  apps::AppProvisioningDataManager::Get()->PopulateFromDynamicUpdate(
      binary_pb, install_dir);

  std::string arc_app_id = "test_arc_app_id";
  std::string web_app_id = "test_web_app_id";
  EntryId arc_entry_id(arc_app_id, apps::AppType::kArc);
  EntryId web_entry_id(web_app_id, apps::AppType::kWeb);

  EXPECT_THAT(service->GetDuplicates(arc_entry_id),
              ElementsAre(Entry(arc_entry_id), Entry(web_entry_id)));
  EXPECT_THAT(service->GetDuplicates(web_entry_id),
              ElementsAre(Entry(arc_entry_id), Entry(web_entry_id)));
  EXPECT_TRUE(service->AreDuplicates(arc_entry_id, web_entry_id));

  EntryId not_duplicate_app_id("not_duplicate_app_id", apps::AppType::kWeb);
  EXPECT_TRUE(service->GetDuplicates(not_duplicate_app_id).empty());
  EXPECT_FALSE(service->AreDuplicates(not_duplicate_app_id, web_entry_id));
}

}  // namespace apps::deduplication
