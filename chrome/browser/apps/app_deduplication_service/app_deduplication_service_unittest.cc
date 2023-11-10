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
      Entry(skype_android_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  std::string skype_web_id = "https://web.skype.com/";
  it = service->entry_to_group_map_.find(Entry(skype_web_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  it = service->entry_to_group_map_.find(Entry(GURL(skype_web_id)));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(skype_test_index, it->second);

  auto map_it = service->duplication_map_.find(skype_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(skype_android_app_id, AppType::kArc),
                          Entry(skype_web_id, AppType::kWeb),
                          Entry(GURL(skype_web_id))));

  uint32_t duo_test_index = 2;

  std::string duo_android_app_id = "com.google.duo";
  it = service->entry_to_group_map_.find(
      Entry(duo_android_app_id, AppType::kArc));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(duo_test_index, it->second);

  std::string duo_web_app_id = "https://duo.google.com/?lfhs=2";
  it = service->entry_to_group_map_.find(Entry(duo_web_app_id, AppType::kWeb));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(duo_test_index, it->second);

  std::string duo_website_app_id = "https://duo.google.com/?lfhs=2";
  it = service->entry_to_group_map_.find(Entry(GURL(duo_website_app_id)));
  ASSERT_NE(it, service->entry_to_group_map_.end());
  EXPECT_EQ(duo_test_index, it->second);

  map_it = service->duplication_map_.find(duo_test_index);
  ASSERT_FALSE(map_it == service->duplication_map_.end());
  EXPECT_THAT(map_it->second.entries,
              ElementsAre(Entry(duo_android_app_id, AppType::kArc),
                          Entry(duo_web_app_id, AppType::kWeb),
                          Entry(GURL(duo_website_app_id))));
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

  Entry viber_arc_entry(viber_arc_app_id, apps::AppType::kArc);

  EXPECT_TRUE(service->GetDuplicates(viber_arc_entry).empty());
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
