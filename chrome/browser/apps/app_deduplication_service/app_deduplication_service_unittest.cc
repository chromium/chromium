// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_server_connector.h"
#include "chrome/browser/apps/app_deduplication_service/app_deduplication_service_factory.h"
#include "chrome/browser/apps/app_deduplication_service/proto/deduplication_data.pb.h"
#include "chrome/browser/apps/app_provisioning_service/app_provisioning_data_manager.h"
#include "chrome/browser/apps/app_provisioning_service/proto/app_data.pb.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
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

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  // BrowserTaskEnvironment has to be the first member or test will break.
  content::BrowserTaskEnvironment task_environment_;
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

  auto map_it = service->duplication_map_.find(skype_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(skype_arc_app_id, AppType::kArc)),
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

  map_it = service->duplication_map_.find(whatsapp_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(whatsapp_arc_app_id, AppType::kArc)),
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
  EntryId whatsapp_arc_entry_id(whatsapp_arc_app_id, apps::AppType::kArc);
  EntryId whatsapp_web_entry_id(whatsapp_web_app_id, apps::AppType::kWeb);

  EXPECT_THAT(
      service->GetDuplicates(skype_arc_entry_id),
      ElementsAre(Entry(skype_arc_entry_id), Entry(skype_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(skype_web_entry_id),
      ElementsAre(Entry(skype_arc_entry_id), Entry(skype_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(whatsapp_web_entry_id),
      ElementsAre(Entry(whatsapp_arc_entry_id), Entry(whatsapp_web_entry_id)));
  EXPECT_THAT(
      service->GetDuplicates(whatsapp_arc_entry_id),
      ElementsAre(Entry(whatsapp_arc_entry_id), Entry(whatsapp_web_entry_id)));

  EXPECT_TRUE(service->AreDuplicates(skype_arc_entry_id, skype_web_entry_id));
  EXPECT_TRUE(
      service->AreDuplicates(whatsapp_arc_entry_id, whatsapp_web_entry_id));

  EXPECT_FALSE(
      service->AreDuplicates(skype_arc_entry_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_arc_entry_id, whatsapp_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_web_entry_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(skype_web_entry_id, whatsapp_web_entry_id));

  EntryId not_duplicate_app_id("not_duplicate_app_id", apps::AppType::kWeb);
  EXPECT_TRUE(service->GetDuplicates(not_duplicate_app_id).empty());
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_web_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, whatsapp_arc_entry_id));
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, whatsapp_web_entry_id));
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
  EntryId whatsapp_arc_entry_id(whatsapp_arc_app_id, apps::AppType::kArc);
  EntryId whatsapp_web_entry_id(whatsapp_web_app_id, apps::AppType::kWeb);

  // If nothing is installed, should only return phonehub app.
  EXPECT_THAT(service->GetDuplicates(skype_arc_entry_id), ElementsAre());
  EXPECT_THAT(service->GetDuplicates(whatsapp_web_entry_id), ElementsAre());

  UpdateAppReadiness(proxy, "app1", skype_arc_app_id, apps::AppType::kArc,
                     Readiness::kReady);
  EXPECT_THAT(service->GetDuplicates(skype_web_entry_id),
              ElementsAre(Entry(skype_arc_entry_id)));

  UpdateAppReadiness(proxy, "app2", whatsapp_web_app_id, apps::AppType::kWeb,
                     Readiness::kReady);
  EXPECT_THAT(service->GetDuplicates(whatsapp_arc_entry_id),
              ElementsAre(Entry(whatsapp_web_entry_id)));

  // Uninstall the app removes it from duplicates.
  UpdateAppReadiness(proxy, "app1", skype_arc_app_id, apps::AppType::kArc,
                     Readiness::kUninstalledByUser);
  EXPECT_THAT(service->GetDuplicates(skype_arc_entry_id), ElementsAre());
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

  EXPECT_THAT(
      service->GetDuplicates(skype_arc_entry_id),
      ElementsAre(Entry(skype_arc_entry_id), Entry(skype_web_entry_id)));
  EXPECT_TRUE(service->AreDuplicates(skype_arc_entry_id, skype_web_entry_id));

  EntryId not_duplicate_app_id("not_duplicate_app_id", apps::AppType::kWeb);
  EXPECT_TRUE(service->GetDuplicates(not_duplicate_app_id).empty());
  EXPECT_FALSE(
      service->AreDuplicates(not_duplicate_app_id, skype_web_entry_id));
}

class AppDeduplicationServiceAlmanacTest : public testing::Test {
 protected:
  AppDeduplicationServiceAlmanacTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAppDeduplicationServiceFondue);
  }

  void SetUp() override {
    testing::Test::SetUp();
    AppDeduplicationServiceFactory::SkipApiKeyCheckForTesting(true);

    TestingProfile::Builder profile_builder;
    profile_builder.SetSharedURLLoaderFactory(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &url_loader_factory_));
    profile_ = profile_builder.Build();
  }

  void TearDown() override {
    AppDeduplicationServiceFactory::SkipApiKeyCheckForTesting(false);
  }

  Profile* GetProfile() { return profile_.get(); }

  network::TestURLLoaderFactory url_loader_factory_;

 private:
  // BrowserTaskEnvironment has to be the first member or test will break.
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ash::system::ScopedFakeStatisticsProvider fake_statistics_provider_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(AppDeduplicationServiceAlmanacTest, DeduplicateDataToEntries) {
  proto::DeduplicateData data;

  auto* skype_group = data.add_app_group();
  skype_group->set_app_group_uuid("15ca3ac3-c8cd-4a0c-a195-2ea210ea922c");
  skype_group->add_package_id();
  skype_group->set_package_id(0, "android:com.skype.raider");
  skype_group->add_package_id();
  skype_group->set_package_id(1, "web:https://web.skype.com/");
  skype_group->add_package_id();
  skype_group->set_package_id(2, "website:https://web.skype.com/");

  auto* duo_group = data.add_app_group();
  duo_group->set_app_group_uuid("1d460a2b-d6d5-471d-b1e6-bbfc87971ea8");
  duo_group->add_package_id();
  duo_group->set_package_id(0, "android:com.google.duo");
  duo_group->add_package_id();
  duo_group->set_package_id(1, "web:https://duo.google.com/?lfhs=2");
  duo_group->add_package_id();
  duo_group->set_package_id(2, "website:https://duo.google.com/?lfhs=2");

  TestingProfile profile;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  // This function is called to populate the duplicate map, or else IsServiceOn
  // will return false.
  service->DeduplicateDataToEntries(data);

  EXPECT_TRUE(service->IsServiceOn());

  uint32_t skype_test_index = 1;

  std::string skype_android_app_id = "com.skype.raider";
  auto it = service->entry_to_group_map_.find(
      EntryId(skype_android_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  std::string skype_web_id = "https://web.skype.com/";
  it = service->entry_to_group_map_.find(EntryId(skype_web_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  it = service->entry_to_group_map_.find(EntryId(GURL(skype_web_id)));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  auto map_it = service->duplication_map_.find(skype_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(skype_android_app_id, AppType::kArc)),
                          Entry(EntryId(skype_web_id, AppType::kWeb)),
                          Entry(EntryId(GURL(skype_web_id)))));

  uint32_t duo_test_index = 2;

  std::string duo_android_app_id = "com.google.duo";
  it = service->entry_to_group_map_.find(
      EntryId(duo_android_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(duo_test_index, it->second);

  std::string duo_web_app_id = "https://duo.google.com/?lfhs=2";
  it =
      service->entry_to_group_map_.find(EntryId(duo_web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(duo_test_index, it->second);

  std::string duo_website_app_id = "https://duo.google.com/?lfhs=2";
  it = service->entry_to_group_map_.find(EntryId(GURL(duo_website_app_id)));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(duo_test_index, it->second);

  map_it = service->duplication_map_.find(duo_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(EntryId(duo_android_app_id, AppType::kArc)),
                          Entry(EntryId(duo_web_app_id, AppType::kWeb)),
                          Entry(EntryId(GURL(duo_website_app_id)))));
}

TEST_F(AppDeduplicationServiceAlmanacTest,
       DeduplicateDataToEntriesInvalidAppType) {
  proto::DeduplicateData data;

  auto* skype_group = data.add_app_group();
  skype_group->set_app_group_uuid("15ca3ac3-c8cd-4a0c-a195-2ea210ea922c");
  skype_group->add_package_id();
  skype_group->set_package_id(0, "notatype:com.skype.raider");

  TestingProfile profile;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  // This function is called to populate the duplicate map, or else IsServiceOn
  // will return false.
  service->DeduplicateDataToEntries(data);

  // IsServiceOn should return false as the duplicate map will be empty as it
  // has received invalid data for population.
  EXPECT_FALSE(service->IsServiceOn());
}

TEST_F(AppDeduplicationServiceAlmanacTest,
       DeduplicateDataToEntriesInvalidAppId) {
  proto::DeduplicateData data;

  auto* skype_group = data.add_app_group();
  skype_group->set_app_group_uuid("15ca3ac3-c8cd-4a0c-a195-2ea210ea922c");
  skype_group->add_package_id();
  skype_group->set_package_id(0, "website:notavalidwebsite");

  TestingProfile profile;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);
  EXPECT_NE(nullptr, service);

  // This function is called to populate the duplicate map, or else IsServiceOn
  // will return false.
  service->DeduplicateDataToEntries(data);

  // IsServiceOn should return false as the duplicate map will be empty as it
  // has received invalid data for population.
  EXPECT_FALSE(service->IsServiceOn());
}

TEST_F(AppDeduplicationServiceAlmanacTest, PrefUnchangedAfterServerError) {
  url_loader_factory_.AddResponse(
      AppDeduplicationServerConnector::GetServerUrl().spec(), /*content=*/"",
      net::HTTP_INTERNAL_SERVER_ERROR);

  base::test::TestFuture<bool> result;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(GetProfile()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(GetProfile());
  EXPECT_NE(nullptr, service);

  base::Time time_before = service->GetServerPref();

  service->GetDeduplicateAppsCompleteCallbackForTesting(result.GetCallback());
  ASSERT_FALSE(result.Get());

  base::Time time_after = service->GetServerPref();
  EXPECT_EQ(time_before, time_after);
}

TEST_F(AppDeduplicationServiceAlmanacTest, PrefSetAfterServerSuccess) {
  proto::DeduplicateData data;
  auto* group = data.add_app_group();
  group->set_app_group_uuid("15ca3ac3-c8cd-4a0c-a195-2ea210ea922c");
  group->add_package_id();
  group->set_package_id(0, "website:https://web.skype.com/");

  url_loader_factory_.AddResponse(
      AppDeduplicationServerConnector::GetServerUrl().spec(),
      data.SerializeAsString());

  base::test::TestFuture<bool> result;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(GetProfile()));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(GetProfile());
  EXPECT_NE(nullptr, service);

  base::Time time_before = service->GetServerPref();

  service->GetDeduplicateAppsCompleteCallbackForTesting(result.GetCallback());
  ASSERT_TRUE(result.Get());

  base::Time time_after = service->GetServerPref();
  EXPECT_TRUE(time_before < time_after);
}

TEST_F(AppDeduplicationServiceAlmanacTest, ValidServiceNoDuplicates) {
  proto::DeduplicateData data;

  auto* viber_group = data.add_app_group();
  viber_group->set_app_group_uuid("af45163b-111d-4d43-b191-01a9f8aece4c");
  viber_group->add_package_id();
  viber_group->set_package_id(0, "android:com.viber.voip");

  std::string viber_arc_app_id = "com.viber.voip";

  TestingProfile profile;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);

  EXPECT_NE(nullptr, service);

  // This function is called to populate the duplicate map, or else IsServiceOn
  // will return false.
  service->DeduplicateDataToEntries(data);

  EXPECT_TRUE(service->IsServiceOn());

  EntryId viber_arc_entry_id(viber_arc_app_id, apps::AppType::kArc);

  EXPECT_TRUE(service->GetDuplicates(viber_arc_entry_id).empty());
}

TEST_F(AppDeduplicationServiceAlmanacTest, InvalidServiceNoDuplicates) {
  TestingProfile profile;
  ASSERT_TRUE(AppDeduplicationServiceFactory::
                  IsAppDeduplicationServiceAvailableForProfile(&profile));
  auto* service = AppDeduplicationServiceFactory::GetForProfile(&profile);

  EXPECT_NE(nullptr, service);

  EXPECT_FALSE(service->IsServiceOn());
}
}  // namespace apps::deduplication
