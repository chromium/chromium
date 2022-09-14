// Copyright 2022 The Chromium Authors
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
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
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

  void UpdateAppReadiness(AppServiceProxy* proxy,
                          const std::string& app_id,
                          const std::string& publisher_id,
                          apps::AppType app_type,
                          Readiness readiness) {
    AppPtr app = std::make_unique<apps::App>(app_type, app_id);
    app->publisher_id = publisher_id;
    app->readiness = readiness;
    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    proxy->AppRegistryCache().OnApps(std::move(apps), app_type,
                                     /*should_notify_initialized=*/false);
  }

  absl::optional<proto::DuplicatedGroupList> ReadProtoFromFile() {
    base::FilePath path;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &path)) {
      return absl::nullopt;
    }
    path = path.AppendASCII("app_deduplication_service/binary_test_data.pb");

    std::string dedupe_pb;
    if (!base::ReadFileToString(path, &dedupe_pb)) {
      return absl::nullopt;
    }

    proto::DuplicatedGroupList duplicated_group_list;
    if (!duplicated_group_list.ParseFromString(dedupe_pb)) {
      return absl::nullopt;
    }
    return duplicated_group_list;
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

TEST_F(AppDeduplicationServiceTest, OnDuplicatedGroupListUpdated) {
  absl::optional<proto::DuplicatedGroupList> duplicated_group_list =
      ReadProtoFromFile();
  ASSERT_TRUE(duplicated_group_list.has_value());

  TestingProfile profile;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  service->OnDuplicatedGroupListUpdated(duplicated_group_list.value());

  uint32_t skype_test_index = 1;
  std::string skype_arc_app_id = "com.skype.raider";
  auto it = service->entry_to_group_map_.find(
      EntryId(skype_arc_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  std::string skype_web_app_id = "https://web.skype.com/";
  it = service->entry_to_group_map_.find(
      EntryId(skype_web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  std::string skype_phonehub_app_id = "com.skype.raider";
  it = service->entry_to_group_map_.find(EntryId(skype_phonehub_app_id));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  auto map_it = service->duplication_map_.find(skype_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(skype_phonehub_app_id)),
                          Entry(EntryId(skype_arc_app_id, AppType::kArc)),
                          Entry(EntryId(skype_web_app_id, AppType::kWeb))));

  uint32_t whatsapp_test_index = 2;
  std::string whatsapp_arc_app_id = "com.whatsapp";
  it = service->entry_to_group_map_.find(
      EntryId(whatsapp_arc_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(whatsapp_test_index, it->second);

  std::string whatsapp_web_app_id = "https://web.whatsapp.com/";
  it = service->entry_to_group_map_.find(
      EntryId(whatsapp_web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(whatsapp_test_index, it->second);

  std::string whatsapp_phonehub_app_id = "com.whatsapp";
  it = service->entry_to_group_map_.find(EntryId(whatsapp_phonehub_app_id));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(whatsapp_test_index, it->second);

  map_it = service->duplication_map_.find(whatsapp_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(whatsapp_phonehub_app_id)),
                          Entry(EntryId(whatsapp_arc_app_id, AppType::kArc)),
                          Entry(EntryId(whatsapp_web_app_id, AppType::kWeb))));
}

// Test that if all apps in the duplicated group are installed, the full list
// will be returned.
TEST_F(AppDeduplicationServiceTest, ExactDuplicateAllInstalled) {
  absl::optional<proto::DuplicatedGroupList> duplicated_group_list =
      ReadProtoFromFile();
  ASSERT_TRUE(duplicated_group_list.has_value());

  TestingProfile profile;

  // Set up app installed.
  auto* proxy = AppServiceProxyFactory::GetForProfile(&profile);
  std::string skype_arc_app_id = "com.skype.raider";
  std::string skype_web_app_id = "https://web.skype.com/";
  std::string whatsapp_arc_app_id = "com.whatsapp";
  std::string whatsapp_web_app_id = "https://web.whatsapp.com/";

  UpdateAppReadiness(proxy, "app1", skype_arc_app_id, apps::AppType::kArc,
                     Readiness::kReady);
  UpdateAppReadiness(proxy, "app2", skype_web_app_id, apps::AppType::kWeb,
                     Readiness::kReady);
  UpdateAppReadiness(proxy, "app3", whatsapp_arc_app_id, apps::AppType::kArc,
                     Readiness::kReady);
  UpdateAppReadiness(proxy, "app4", whatsapp_web_app_id, apps::AppType::kWeb,
                     Readiness::kReady);

  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  service->OnDuplicatedGroupListUpdated(duplicated_group_list.value());

  EntryId skype_arc_entry_id(skype_arc_app_id, apps::AppType::kArc);
  EntryId skype_web_entry_id(skype_web_app_id, apps::AppType::kWeb);
  EntryId skype_phonehub_entry_id("com.skype.raider");
  EntryId whatsapp_arc_entry_id(whatsapp_arc_app_id, apps::AppType::kArc);
  EntryId whatsapp_web_entry_id(whatsapp_web_app_id, apps::AppType::kWeb);
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

TEST_F(AppDeduplicationServiceTest, Installation) {
  absl::optional<proto::DuplicatedGroupList> duplicated_group_list =
      ReadProtoFromFile();
  ASSERT_TRUE(duplicated_group_list.has_value());

  TestingProfile profile;

  auto* proxy = AppServiceProxyFactory::GetForProfile(&profile);

  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  service->OnDuplicatedGroupListUpdated(duplicated_group_list.value());

  std::string skype_arc_app_id = "com.skype.raider";
  std::string skype_web_app_id = "https://web.skype.com/";
  std::string whatsapp_arc_app_id = "com.whatsapp";
  std::string whatsapp_web_app_id = "https://web.whatsapp.com/";

  EntryId skype_arc_entry_id(skype_arc_app_id, apps::AppType::kArc);
  EntryId skype_web_entry_id(skype_web_app_id, apps::AppType::kWeb);
  EntryId skype_phonehub_entry_id("com.skype.raider");
  EntryId whatsapp_arc_entry_id(whatsapp_arc_app_id, apps::AppType::kArc);
  EntryId whatsapp_web_entry_id(whatsapp_web_app_id, apps::AppType::kWeb);
  EntryId whatsapp_phonehub_entry_id("com.whatsapp");

  // If nothing is installed, should only return phonehub app.
  EXPECT_THAT(service->GetDuplicates(skype_arc_entry_id),
              ElementsAre(Entry(skype_phonehub_entry_id)));
  EXPECT_THAT(service->GetDuplicates(whatsapp_web_entry_id),
              ElementsAre(Entry(whatsapp_phonehub_entry_id)));

  UpdateAppReadiness(proxy, "app1", skype_arc_app_id, apps::AppType::kArc,
                     Readiness::kReady);
  EXPECT_THAT(
      service->GetDuplicates(skype_web_entry_id),
      ElementsAre(Entry(skype_phonehub_entry_id), Entry(skype_arc_entry_id)));

  UpdateAppReadiness(proxy, "app2", whatsapp_web_app_id, apps::AppType::kWeb,
                     Readiness::kReady);
  EXPECT_THAT(service->GetDuplicates(whatsapp_arc_entry_id),
              ElementsAre(Entry(whatsapp_phonehub_entry_id),
                          Entry(whatsapp_web_entry_id)));

  // Uninstall the app removes it from duplicates.
  UpdateAppReadiness(proxy, "app1", skype_arc_app_id, apps::AppType::kArc,
                     Readiness::kUninstalledByUser);
  EXPECT_THAT(service->GetDuplicates(skype_arc_entry_id),
              ElementsAre(Entry(skype_phonehub_entry_id)));
}

TEST_F(AppDeduplicationServiceTest, Websites) {
  absl::optional<proto::DuplicatedGroupList> duplicated_group_list =
      ReadProtoFromFile();
  ASSERT_TRUE(duplicated_group_list.has_value());

  TestingProfile profile;

  auto* proxy = AppServiceProxyFactory::GetForProfile(&profile);

  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  service->OnDuplicatedGroupListUpdated(duplicated_group_list.value());

  GURL keep_website = GURL("https://keep.google.com/");
  GURL keep_website_with_path = GURL("https://keep.google.com/testtesttest");
  GURL not_keep_website = GURL("https://www.google.com/");
  std::string keep_arc_app_id = "com.google.android.keep";
  std::string keep_web_app_id = "https://keep.google.com/?usp=installed_webapp";
  GURL wrong_scheme_website = GURL("http://www.google.com/");

  EntryId keep_website_entry_id(keep_website);
  EntryId keep_website_with_path_entry_id(keep_website_with_path);
  EntryId keep_arc_entry_id(keep_arc_app_id, apps::AppType::kArc);
  EntryId keep_web_entry_id(keep_web_app_id, apps::AppType::kWeb);
  EntryId not_keep_website_entry_id(not_keep_website);
  EntryId wrong_scheme_website_entry_id(wrong_scheme_website);

  UpdateAppReadiness(proxy, "app1", keep_arc_app_id, apps::AppType::kArc,
                     Readiness::kReady);
  UpdateAppReadiness(proxy, "app2", keep_web_app_id, apps::AppType::kWeb,
                     Readiness::kReady);

  EXPECT_THAT(service->GetDuplicates(keep_website_entry_id),
              ElementsAre(Entry(keep_arc_entry_id), Entry(keep_web_entry_id),
                          Entry(keep_website_entry_id)));
  EXPECT_THAT(service->GetDuplicates(keep_website_with_path_entry_id),
              ElementsAre(Entry(keep_arc_entry_id), Entry(keep_web_entry_id),
                          Entry(keep_website_entry_id)));
  EXPECT_THAT(service->GetDuplicates(not_keep_website_entry_id),
              testing::IsEmpty());
  EXPECT_THAT(service->GetDuplicates(wrong_scheme_website_entry_id),
              testing::IsEmpty());

  EXPECT_TRUE(service->AreDuplicates(keep_website_with_path_entry_id,
                                     keep_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_keep_website_entry_id, keep_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(wrong_scheme_website_entry_id, keep_web_entry_id));
}

// Test updating duplication data from app provisioning data manager.
TEST_F(AppDeduplicationServiceTest, AppPromisioningDataManagerUpdate) {
  TestingProfile profile;

  auto* proxy = AppServiceProxyFactory::GetForProfile(&profile);
  std::string skype_arc_app_id = "com.skype.raider";
  std::string skype_web_app_id = "https://web.skype.com/";
  UpdateAppReadiness(proxy, "app1", skype_arc_app_id, apps::AppType::kArc,
                     Readiness::kReady);
  UpdateAppReadiness(proxy, "app2", skype_web_app_id, apps::AppType::kWeb,
                     Readiness::kReady);

  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  std::string app_with_locale_pb = "";
  base::FilePath install_dir("/");
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("app_deduplication_service/binary_test_data.pb");
  std::string dedupe_pb;
  ASSERT_TRUE(base::ReadFileToString(path, &dedupe_pb));

  ComponentFileContents component_files = {app_with_locale_pb, dedupe_pb};
  apps::AppProvisioningDataManager::Get()->PopulateFromDynamicUpdate(
      component_files, install_dir);

  EntryId skype_arc_entry_id(skype_arc_app_id, apps::AppType::kArc);
  EntryId skype_web_entry_id(skype_web_app_id, apps::AppType::kWeb);
  EntryId skype_phonehub_entry_id("com.skype.raider");

  EXPECT_THAT(
      service->GetDuplicates(skype_arc_entry_id),
      ElementsAre(Entry(skype_phonehub_entry_id), Entry(skype_arc_entry_id),
                  Entry(skype_web_entry_id)));
  EXPECT_TRUE(service->AreDuplicates(skype_arc_entry_id, skype_web_entry_id));

  EntryId not_duplicate_app_id("not_duplicate_app_id", apps::AppType::kWeb);
  EXPECT_TRUE(service->GetDuplicates(not_duplicate_app_id).empty());
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_web_entry_id));
}

}  // namespace apps::deduplication
