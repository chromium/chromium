// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "chrome/browser/sync/cross_device_theme_tracker_factory.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/test/test_data_type_store_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
#include "components/themes/cross_device/cross_device_theme_sync_bridge.h"
#include "components/themes/cross_device/theme_comparer.h"
#include "components/themes/cross_device/theme_translation.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace themes {

namespace {

using LocalSpecifics = sync_pb::ThemeSpecifics;

class MockObserver : public CrossDeviceThemeTracker<LocalSpecifics>::Observer {
 public:
  MOCK_METHOD(void, OnCrossDeviceThemeChanged, (), (override));
  MOCK_METHOD(void, OnServiceStatusChanged, (ServiceStatus), (override));
};

class CrossDeviceThemeTrackerDesktopTest : public testing::Test {
 protected:
  CrossDeviceThemeTrackerDesktopTest() {
    tracker_ = std::make_unique<CrossDeviceThemeTracker<LocalSpecifics>>(
        &fake_device_info_tracker_);

    // Construct and register Android bridge.
    {
      syncer::RepeatingDataTypeStoreFactory store_factory =
          test_store_service_.GetStoreFactory();
      auto android_processor =
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::THEMES_ANDROID, base::DoNothing());
      auto android_bridge = std::make_unique<themes::CrossDeviceThemeSyncBridge<
          sync_pb::ThemeAndroidSpecifics, LocalSpecifics>>(
          syncer::THEMES_ANDROID,
          base::BindRepeating(&themes::TranslateAndroid),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::UpdateThemeInfo,
              base::Unretained(tracker_.get())),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::RemoveThemeInfo,
              base::Unretained(tracker_.get())),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::OnBridgeSyncStarted,
              base::Unretained(tracker_.get()), syncer::THEMES_ANDROID),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::OnBridgeSyncDisabled,
              base::Unretained(tracker_.get()), syncer::THEMES_ANDROID),
          std::move(android_processor), store_factory);
      android_bridge_ = android_bridge.get();
      tracker_->RegisterBridge(syncer::THEMES_ANDROID,
                               std::move(android_bridge));
    }

    // Construct and register iOS bridge.
    {
      syncer::RepeatingDataTypeStoreFactory store_factory =
          test_store_service_.GetStoreFactory();
      auto ios_processor =
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::THEMES_IOS, base::DoNothing());
      auto ios_bridge = std::make_unique<themes::CrossDeviceThemeSyncBridge<
          sync_pb::ThemeIosSpecifics, LocalSpecifics>>(
          syncer::THEMES_IOS, base::BindRepeating(&themes::TranslateIos),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::UpdateThemeInfo,
              base::Unretained(tracker_.get())),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::RemoveThemeInfo,
              base::Unretained(tracker_.get())),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::OnBridgeSyncStarted,
              base::Unretained(tracker_.get()), syncer::THEMES_IOS),
          base::BindRepeating(
              &CrossDeviceThemeTracker<LocalSpecifics>::OnBridgeSyncDisabled,
              base::Unretained(tracker_.get()), syncer::THEMES_IOS),
          std::move(ios_processor), store_factory);
      ios_bridge_ = ios_bridge.get();
      tracker_->RegisterBridge(syncer::THEMES_IOS, std::move(ios_bridge));
    }

    // Wait for bridges to be ready.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return android_bridge_->IsStoreInitializedForTesting() &&
             ios_bridge_->IsStoreInitializedForTesting();
    }));
  }

  ~CrossDeviceThemeTrackerDesktopTest() override {
    android_bridge_ = nullptr;
    ios_bridge_ = nullptr;
  }

  std::string AddDevice(const std::string& cache_guid,
                        const std::string& client_name,
                        syncer::DeviceInfo::OsType os_type,
                        syncer::DeviceInfo::FormFactor form_factor) {
    auto device_info = syncer::TestDeviceInfoBuilder()
                           .WithGuid(cache_guid)
                           .WithClientName(client_name)
                           .WithOsType(os_type)
                           .WithFormFactor(form_factor)
                           .Build();
    fake_device_info_tracker_.Add(std::move(device_info));

    syncer::DataType type = OsTypeToDataType(os_type);
    return syncer::ClientTagHash::FromUnhashed(type, cache_guid).value();
  }

  std::unique_ptr<syncer::EntityChange> CreateAddChange(
      const std::string& storage_key,
      const sync_pb::EntitySpecifics& specifics) {
    syncer::EntityData data;
    data.specifics = specifics;
    data.client_tag_hash = syncer::ClientTagHash::FromHashed(storage_key);
    return syncer::EntityChange::CreateAdd(storage_key, std::move(data));
  }

  base::test::TaskEnvironment task_environment_;
  syncer::FakeDeviceInfoTracker fake_device_info_tracker_;
  syncer::TestDataTypeStoreService test_store_service_;
  std::unique_ptr<CrossDeviceThemeTracker<LocalSpecifics>> tracker_;
  raw_ptr<themes::CrossDeviceThemeSyncBridge<sync_pb::ThemeAndroidSpecifics,
                                             LocalSpecifics>>
      android_bridge_ = nullptr;
  raw_ptr<themes::CrossDeviceThemeSyncBridge<sync_pb::ThemeIosSpecifics,
                                             LocalSpecifics>>
      ios_bridge_ = nullptr;
};

TEST_F(CrossDeviceThemeTrackerDesktopTest, InitialState) {
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);
  EXPECT_TRUE(tracker_->GetOtherDevicesThemes().empty());
}

TEST_F(CrossDeviceThemeTrackerDesktopTest, SyncStarted) {
  MockObserver observer;
  tracker_->AddObserver(&observer);

  // We expect status change notifications.
  // Initially, both are kInitializing.
  // When first bridge starts, aggregate is still kInitializing (no
  // notification). When second bridge starts, aggregate becomes kActive
  // (notifies).
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);

  syncer::DataTypeActivationRequest request;
  android_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  ios_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kActive);

  tracker_->RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerDesktopTest, AndroidThemeUpdate) {
  MockObserver observer;
  tracker_->AddObserver(&observer);

  std::string cache_guid = "android_device_1";
  std::string storage_key = AddDevice(cache_guid, "Android Phone",
                                      syncer::DeviceInfo::OsType::kAndroid,
                                      syncer::DeviceInfo::FormFactor::kPhone);

  // Prepare Android theme specifics
  sync_pb::EntitySpecifics specifics;
  sync_pb::ThemeAndroidSpecifics* android_theme =
      specifics.mutable_theme_android();
  android_theme->set_use_custom_theme(false);
  android_theme->mutable_user_color_theme()->set_color(SK_ColorBLUE);
  android_theme->mutable_user_color_theme()->set_browser_color_variant(
      sync_pb::UserColorTheme::TONAL_SPOT);

  // Simulate sync update
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddChange(storage_key, specifics));

  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  android_bridge_->ApplyIncrementalSyncChanges(
      android_bridge_->CreateMetadataChangeList(), std::move(change_list));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify tracker state
  auto themes = tracker_->GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Android Phone");
  EXPECT_EQ(themes[0].os_type, syncer::DeviceInfo::OsType::kAndroid);
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kPhone);

  // Verify translated theme (ThemeSpecifics)
  const sync_pb::ThemeSpecifics& translated = themes[0].theme;
  EXPECT_FALSE(translated.use_custom_theme());
  ASSERT_TRUE(translated.has_user_color_theme());
  EXPECT_EQ(translated.user_color_theme().color(), SK_ColorBLUE);
  EXPECT_EQ(translated.user_color_theme().browser_color_variant(),
            sync_pb::UserColorTheme::TONAL_SPOT);

  tracker_->RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerDesktopTest, IosThemeUpdate) {
  MockObserver observer;
  tracker_->AddObserver(&observer);

  std::string cache_guid = "ios_device_1";
  std::string storage_key =
      AddDevice(cache_guid, "iPad", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kTablet);

  // Prepare iOS theme specifics
  sync_pb::EntitySpecifics specifics;
  sync_pb::ThemeIosSpecifics* ios_theme = specifics.mutable_theme_ios();
  ios_theme->mutable_user_color_theme()->set_color(SK_ColorGREEN);
  ios_theme->mutable_user_color_theme()->set_browser_color_variant(
      sync_pb::UserColorTheme::NEUTRAL);

  // Simulate sync update
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddChange(storage_key, specifics));

  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  ios_bridge_->ApplyIncrementalSyncChanges(
      ios_bridge_->CreateMetadataChangeList(), std::move(change_list));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify tracker state
  auto themes = tracker_->GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "iPad");
  EXPECT_EQ(themes[0].os_type, syncer::DeviceInfo::OsType::kIOS);
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kTablet);

  // Verify translated theme
  const sync_pb::ThemeSpecifics& translated = themes[0].theme;
  ASSERT_TRUE(translated.has_user_color_theme());
  EXPECT_EQ(translated.user_color_theme().color(), SK_ColorGREEN);
  EXPECT_EQ(translated.user_color_theme().browser_color_variant(),
            sync_pb::UserColorTheme::NEUTRAL);

  tracker_->RemoveObserver(&observer);
}

}  // namespace

}  // namespace themes
