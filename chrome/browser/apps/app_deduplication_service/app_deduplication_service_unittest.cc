// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service_factory.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
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
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("app_deduplication_service/binary_test_data.pb");

  std::string dedupe_pb;
  ASSERT_TRUE(base::ReadFileToString(path, &dedupe_pb));

  proto::DuplicatedAppsMap duplicated_apps_map;
  ASSERT_TRUE(duplicated_apps_map.ParseFromString(dedupe_pb));

  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  service->OnDuplicatedAppsMapUpdated(duplicated_apps_map);

  std::string skype_test_key = "Skype";
  std::string skype_arc_app_id = "com.skype.raider";
  auto it = service->entry_to_group_map_.find(
      EntryId(skype_arc_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_key, it->second);

  std::string skype_web_app_id = "http://web.skype.com/";
  it = service->entry_to_group_map_.find(
      EntryId(skype_web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_key, it->second);

  std::string skype_phonehub_app_id = "com.skype.raider";
  it = service->entry_to_group_map_.find(EntryId(skype_phonehub_app_id));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_key, it->second);

  auto map_it = service->duplication_map_.find(skype_test_key);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(skype_phonehub_app_id)),
                          Entry(EntryId(skype_arc_app_id, AppType::kArc)),
                          Entry(EntryId(skype_web_app_id, AppType::kWeb))));

  std::string whatsapp_test_key = "WhatsApp";
  std::string whatsapp_arc_app_id = "com.whatsapp";
  it = service->entry_to_group_map_.find(
      EntryId(whatsapp_arc_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(whatsapp_test_key, it->second);

  std::string whatsapp_web_app_id = "http://web.whatsapp.com/";
  it = service->entry_to_group_map_.find(
      EntryId(whatsapp_web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(whatsapp_test_key, it->second);

  std::string whatsapp_phonehub_app_id = "com.whatsapp";
  it = service->entry_to_group_map_.find(EntryId(whatsapp_phonehub_app_id));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(whatsapp_test_key, it->second);

  map_it = service->duplication_map_.find(whatsapp_test_key);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(whatsapp_phonehub_app_id)),
                          Entry(EntryId(whatsapp_arc_app_id, AppType::kArc)),
                          Entry(EntryId(whatsapp_web_app_id, AppType::kWeb))));
}

TEST_F(AppDeduplicationServiceTest, ExactDuplicate) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("app_deduplication_service/binary_test_data.pb");

  std::string dedupe_pb;
  ASSERT_TRUE(base::ReadFileToString(path, &dedupe_pb));

  proto::DuplicatedAppsMap duplicated_apps_map;
  ASSERT_TRUE(duplicated_apps_map.ParseFromString(dedupe_pb));

  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  service->OnDuplicatedAppsMapUpdated(duplicated_apps_map);

  EntryId skype_arc_entry_id("com.skype.raider", apps::AppType::kArc);
  EntryId skype_web_entry_id("http://web.skype.com/", apps::AppType::kWeb);
  EntryId skype_phonehub_entry_id("com.skype.raider");
  EntryId whatsapp_arc_entry_id("com.whatsapp", apps::AppType::kArc);
  EntryId whatsapp_web_entry_id("http://web.whatsapp.com/",
                                apps::AppType::kWeb);
  EntryId whatsapp_phonehub_entry_id("com.whatsapp");

  EXPECT_THAT(
      service->GetDuplicates(skype_arc_entry_id),
      ElementsAre(Entry(skype_phonehub_entry_id), Entry(skype_arc_entry_id),
                  Entry(skype_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(skype_web_entry_id),
      ElementsAre(Entry(skype_phonehub_entry_id), Entry(skype_arc_entry_id),
                  Entry(skype_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(skype_phonehub_entry_id),
      ElementsAre(Entry(skype_phonehub_entry_id), Entry(skype_arc_entry_id),
                  Entry(skype_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(whatsapp_web_entry_id),
      ElementsAre(Entry(whatsapp_phonehub_entry_id),
                  Entry(whatsapp_arc_entry_id), Entry(whatsapp_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(whatsapp_arc_entry_id),
      ElementsAre(Entry(whatsapp_phonehub_entry_id),
                  Entry(whatsapp_arc_entry_id), Entry(whatsapp_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(whatsapp_phonehub_entry_id),
      ElementsAre(Entry(whatsapp_phonehub_entry_id),
                  Entry(whatsapp_arc_entry_id), Entry(whatsapp_web_entry_id)));

  EXPECT_TRUE(service->AreDuplicates(skype_arc_entry_id, skype_web_entry_id));
  EXPECT_TRUE(
      service->AreDuplicates(skype_arc_entry_id, skype_phonehub_entry_id));
  EXPECT_TRUE(
      service->AreDuplicates(skype_phonehub_entry_id, skype_web_entry_id));
  EXPECT_TRUE(
      service->AreDuplicates(whatsapp_arc_entry_id, whatsapp_web_entry_id));
  EXPECT_TRUE(service->AreDuplicates(whatsapp_arc_entry_id,
                                     whatsapp_phonehub_entry_id));
  EXPECT_TRUE(service->AreDuplicates(whatsapp_web_entry_id,
                                     whatsapp_phonehub_entry_id));

  EXPECT_FALSE(
      service->AreDuplicates(skype_arc_entry_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_arc_entry_id, whatsapp_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_arc_entry_id, whatsapp_phonehub_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_web_entry_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_web_entry_id, whatsapp_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_web_entry_id, whatsapp_phonehub_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_phonehub_entry_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_phonehub_entry_id, whatsapp_web_entry_id));
  EXPECT_FALSE(service->AreDuplicates(skype_phonehub_entry_id,
                                      whatsapp_phonehub_entry_id));

  EntryId not_duplicate_app_id("not_duplicate_app_id", apps::AppType::kWeb);
  EXPECT_TRUE(service->GetDuplicates(not_duplicate_app_id).empty());
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_phonehub_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, whatsapp_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, whatsapp_phonehub_entry_id));
}

// Test updating duplication data from app provisioning data manager.
TEST_F(AppDeduplicationServiceTest, AppPromisioningDataManagerUpdate) {
  TestingProfile::Builder profile_builder;
  auto profile = profile_builder.Build();
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(profile.get()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(profile.get());
  EXPECT_NE(nullptr, service);

  std::string binary_pb = "";
  base::FilePath install_dir("/");
  // TODO(b/238394602): Move the fake data population to test only when real
  // data feeds in.
  apps::AppProvisioningDataManager::Get()->PopulateFromDynamicUpdate(
      binary_pb, install_dir);

  EntryId arc_entry_id("test_arc_app_id", apps::AppType::kArc);
  EntryId web_entry_id("test_web_app_id", apps::AppType::kWeb);

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
