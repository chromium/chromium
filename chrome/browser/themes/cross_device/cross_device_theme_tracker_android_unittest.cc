// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "chrome/browser/sync/cross_device_theme_tracker_factory.h"
#include "chrome/browser/themes/cross_device/cross_device_theme_tracker_test_util.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"
#include "components/sync/protocol/theme_ios_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/themes/cross_device/theme_translation.h"
#include "third_party/skia/include/core/SkColor.h"

namespace themes {

namespace {

using LocalSpecifics = sync_pb::ThemeAndroidSpecifics;

class MockObserver : public CrossDeviceThemeTracker<LocalSpecifics>::Observer {
 public:
  MOCK_METHOD(void, OnCrossDeviceThemeChanged, (), (override));
  MOCK_METHOD(void, OnServiceStatusChanged, (ServiceStatus), (override));
};

class CrossDeviceThemeTrackerAndroidTest
    : public CrossDeviceThemeTrackerTestBase<LocalSpecifics> {
 protected:
  CrossDeviceThemeTrackerAndroidTest() {
    desktop_bridge_ = RegisterBridgeHelper<sync_pb::ThemeSpecifics>(
        syncer::THEMES, base::BindRepeating(&themes::TranslateDesktop),
        tracker_.get(), &test_store_service_);

    ios_bridge_ = RegisterBridgeHelper<sync_pb::ThemeIosSpecifics>(
        syncer::THEMES_IOS, base::BindRepeating(&themes::TranslateIos),
        tracker_.get(), &test_store_service_);

    // Wait for bridges to be ready.
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return desktop_bridge_->IsStoreInitializedForTesting() &&
             ios_bridge_->IsStoreInitializedForTesting();
    }));
  }

  ~CrossDeviceThemeTrackerAndroidTest() override {
    desktop_bridge_ = nullptr;
    ios_bridge_ = nullptr;
  }

  raw_ptr<themes::CrossDeviceThemeSyncBridge<sync_pb::ThemeSpecifics,
                                             LocalSpecifics>>
      desktop_bridge_ = nullptr;
  raw_ptr<themes::CrossDeviceThemeSyncBridge<sync_pb::ThemeIosSpecifics,
                                             LocalSpecifics>>
      ios_bridge_ = nullptr;
};

TEST_F(CrossDeviceThemeTrackerAndroidTest, InitialState) {
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);
  EXPECT_TRUE(tracker_->GetOtherDevicesThemes().empty());
}

TEST_F(CrossDeviceThemeTrackerAndroidTest, SyncStarted) {
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
  desktop_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  ios_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kActive);

  tracker_->RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerAndroidTest, DesktopUserThemeUpdate) {
  MockObserver observer;
  tracker_->AddObserver(&observer);

  std::string cache_guid = "desktop_device_1";
  std::string storage_key = AddDevice(cache_guid, "Windows Desktop",
                                      syncer::DeviceInfo::OsType::kWindows,
                                      syncer::DeviceInfo::FormFactor::kDesktop);

  // Prepare Desktop theme specifics with UserColorTheme
  sync_pb::EntitySpecifics specifics;
  sync_pb::ThemeSpecifics* desktop_theme = specifics.mutable_theme();
  desktop_theme->set_use_custom_theme(false);
  desktop_theme->mutable_user_color_theme()->set_color(SK_ColorBLUE);
  desktop_theme->mutable_user_color_theme()->set_browser_color_variant(
      sync_pb::UserColorTheme::TONAL_SPOT);

  // Simulate sync update
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddChange(storage_key, specifics));

  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  desktop_bridge_->ApplyIncrementalSyncChanges(
      desktop_bridge_->CreateMetadataChangeList(), std::move(change_list));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify tracker state
  auto themes = tracker_->GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Windows Desktop");
  // The translated info will have the actual OS type resolved from DeviceInfo.
  EXPECT_EQ(themes[0].os_type, syncer::DeviceInfo::OsType::kWindows);
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kDesktop);

  // Verify translated theme (ThemeAndroidSpecifics)
  const sync_pb::ThemeAndroidSpecifics& translated = themes[0].theme;
  EXPECT_TRUE(translated.use_custom_theme());
  ASSERT_TRUE(translated.has_user_color_theme());
  EXPECT_EQ(translated.user_color_theme().color(), SK_ColorBLUE);
  EXPECT_EQ(translated.user_color_theme().browser_color_variant(),
            sync_pb::UserColorTheme::TONAL_SPOT);

  tracker_->RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerAndroidTest, DesktopAutogeneratedThemeUpdate) {
  MockObserver observer;
  tracker_->AddObserver(&observer);

  std::string cache_guid = "desktop_device_2";
  std::string storage_key =
      AddDevice(cache_guid, "Macbook", syncer::DeviceInfo::OsType::kMac,
                syncer::DeviceInfo::FormFactor::kDesktop);

  // Prepare Desktop theme specifics with AutogeneratedColorTheme
  sync_pb::EntitySpecifics specifics;
  sync_pb::ThemeSpecifics* desktop_theme = specifics.mutable_theme();
  desktop_theme->set_use_custom_theme(false);
  desktop_theme->mutable_autogenerated_color_theme()->set_color(SK_ColorRED);

  // Simulate sync update
  syncer::EntityChangeList change_list;
  change_list.push_back(CreateAddChange(storage_key, specifics));

  EXPECT_CALL(observer, OnCrossDeviceThemeChanged()).Times(1);
  desktop_bridge_->ApplyIncrementalSyncChanges(
      desktop_bridge_->CreateMetadataChangeList(), std::move(change_list));
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Verify tracker state
  auto themes = tracker_->GetOtherDevicesThemes();
  ASSERT_EQ(themes.size(), 1u);
  EXPECT_EQ(themes[0].device_name, "Macbook");
  // The translated info will have the actual OS type resolved from DeviceInfo
  // (kMac), overwriting the kWindows placeholder.
  EXPECT_EQ(themes[0].os_type, syncer::DeviceInfo::OsType::kMac);
  EXPECT_EQ(themes[0].form_factor, syncer::DeviceInfo::FormFactor::kDesktop);

  // Verify translated theme (ThemeAndroidSpecifics)
  const sync_pb::ThemeAndroidSpecifics& translated = themes[0].theme;
  EXPECT_TRUE(translated.use_custom_theme());
  // Autogenerated theme should be translated to UserColorTheme
  ASSERT_TRUE(translated.has_user_color_theme());
  EXPECT_EQ(translated.user_color_theme().color(), SK_ColorRED);
  EXPECT_EQ(translated.user_color_theme().browser_color_variant(),
            sync_pb::UserColorTheme::TONAL_SPOT);

  tracker_->RemoveObserver(&observer);
}

TEST_F(CrossDeviceThemeTrackerAndroidTest, IosThemeUpdate) {
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
  const sync_pb::ThemeAndroidSpecifics& translated = themes[0].theme;
  EXPECT_TRUE(translated.use_custom_theme());
  ASSERT_TRUE(translated.has_user_color_theme());
  EXPECT_EQ(translated.user_color_theme().color(), SK_ColorGREEN);
  EXPECT_EQ(translated.user_color_theme().browser_color_variant(),
            sync_pb::UserColorTheme::NEUTRAL);

  tracker_->RemoveObserver(&observer);
}

}  // namespace

}  // namespace themes
