// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
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
        tracker_.get());

    ios_bridge_ = RegisterBridgeHelper<sync_pb::ThemeIosSpecifics>(
        syncer::THEMES_IOS, base::BindRepeating(&themes::TranslateIos),
        tracker_.get());

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

  // Before initial sync, metadata is not yet tracked, so OnSyncStarting alone
  // should keep the service status in kInitializing until MergeFullSyncData
  // completes on both bridges.
  EXPECT_CALL(observer, OnServiceStatusChanged(ServiceStatus::kActive))
      .Times(1);

  syncer::DataTypeActivationRequest request;
  desktop_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  ios_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  desktop_bridge_->MergeFullSyncData(CreateSyncedMetadata(desktop_bridge_),
                                     syncer::EntityChangeList());
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  ios_bridge_->MergeFullSyncData(CreateSyncedMetadata(ios_bridge_),
                                 syncer::EntityChangeList());
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kActive);

  tracker_->RemoveObserver(&observer);

  // Disabling sync and re-starting before initial sync metadata is tracked
  // should transition the tracker back to kInitializing until MergeFullSyncData
  // completes on both bridges.
  desktop_bridge_->ApplyDisableSyncChanges(
      desktop_bridge_->CreateMetadataChangeList());
  ios_bridge_->ApplyDisableSyncChanges(ios_bridge_->CreateMetadataChangeList());
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kSyncDisabled);

  desktop_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);
  ios_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  // Pausing sync while metadata is not yet tracked should transition the
  // tracker to kSyncDisabled, and re-starting should return to kInitializing.
  desktop_bridge_->OnSyncPaused();
  ios_bridge_->OnSyncPaused();
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kSyncDisabled);

  desktop_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);
  ios_bridge_->OnSyncStarting(request);
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);

  desktop_bridge_->MergeFullSyncData(CreateSyncedMetadata(desktop_bridge_),
                                     syncer::EntityChangeList());
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kInitializing);
  ios_bridge_->MergeFullSyncData(CreateSyncedMetadata(ios_bridge_),
                                 syncer::EntityChangeList());
  EXPECT_EQ(tracker_->GetServiceStatus(), ServiceStatus::kActive);

  // Simulate a restart where initial sync metadata was already persisted.
  auto restarted_tracker =
      std::make_unique<CrossDeviceThemeTracker<LocalSpecifics>>();
  auto* restarted_desktop_bridge =
      RegisterBridgeHelper<sync_pb::ThemeSpecifics>(
          syncer::THEMES, base::BindRepeating(&themes::TranslateDesktop),
          restarted_tracker.get());
  auto* restarted_ios_bridge = RegisterBridgeHelper<sync_pb::ThemeIosSpecifics>(
      syncer::THEMES_IOS, base::BindRepeating(&themes::TranslateIos),
      restarted_tracker.get());

  // Calling OnSyncStarting before the store finishes loading metadata should
  // mark the bridge active once OnReadAllDataAndMetadata completes.
  restarted_desktop_bridge->OnSyncStarting(request);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return restarted_desktop_bridge->IsStoreInitializedForTesting() &&
           restarted_ios_bridge->IsStoreInitializedForTesting();
  }));
  EXPECT_EQ(restarted_tracker->GetServiceStatus(),
            ServiceStatus::kInitializing);

  // Calling OnSyncStarting after metadata is already loaded and tracked should
  // immediately mark the bridge active without MergeFullSyncData.
  restarted_ios_bridge->OnSyncStarting(request);
  EXPECT_EQ(restarted_tracker->GetServiceStatus(), ServiceStatus::kActive);

  // Pausing sync when metadata is already tracked should keep the tracker
  // active.
  restarted_desktop_bridge->OnSyncPaused();
  restarted_ios_bridge->OnSyncPaused();
  EXPECT_EQ(restarted_tracker->GetServiceStatus(), ServiceStatus::kActive);
}

TEST_F(CrossDeviceThemeTrackerAndroidTest, DesktopUserThemeUpdate) {
  MockObserver observer;
  tracker_->AddObserver(&observer);

  std::string storage_key = "current_theme";

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
  EXPECT_EQ(themes[0].data_type, syncer::THEMES);

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

  std::string storage_key = "current_theme";

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
  EXPECT_EQ(themes[0].data_type, syncer::THEMES);

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

  std::string storage_key = "current_theme_ios";

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
  EXPECT_EQ(themes[0].data_type, syncer::THEMES_IOS);

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
