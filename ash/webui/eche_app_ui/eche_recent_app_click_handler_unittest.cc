// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_recent_app_click_handler.h"

#include <string>

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_suite.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/fake_launch_app_helper.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

class EcheRecentAppClickHandlerTest : public AshTestBase {
 protected:
  EcheRecentAppClickHandlerTest() = default;
  EcheRecentAppClickHandlerTest(const EcheRecentAppClickHandlerTest&) = delete;
  EcheRecentAppClickHandlerTest& operator=(
      const EcheRecentAppClickHandlerTest&) = delete;
  ~EcheRecentAppClickHandlerTest() override = default;

  // AshTestBase::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA, features::kPhoneHubRecentApps,
                              features::kEcheCustomWidget},
        /*disabled_features=*/{});

    DCHECK(test_web_view_factory_.get());

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    eche_tray_ =
        ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();

    fake_phone_hub_manager_.fake_feature_status_provider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    fake_feature_status_provider_.SetStatus(FeatureStatus::kIneligible);

    launch_app_helper_ = std::make_unique<FakeLaunchAppHelper>(
        &fake_phone_hub_manager_,
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeLaunchEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeCloseEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheRecentAppClickHandlerTest::FakeLaunchNotificationFunction,
            base::Unretained(this)));
    display_stream_handler_ = std::make_unique<EcheDisplayStreamHandler>();
    handler_ = std::make_unique<EcheRecentAppClickHandler>(
        &fake_phone_hub_manager_, &fake_feature_status_provider_,
        launch_app_helper_.get(), display_stream_handler_.get());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    launch_app_helper_.reset();
    display_stream_handler_.reset();
    handler_.reset();
  }

  void FakeLaunchEcheAppFunction(const absl::optional<int64_t>& notification_id,
                                 const std::string& package_name,
                                 const std::u16string& visible_name,
                                 const absl::optional<int64_t>& user_id,
                                 const gfx::Image& icon) {
    package_name_ = package_name;
    visible_name_ = visible_name;
    user_id_ = user_id.value();
  }

  void FakeLaunchNotificationFunction(
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    // Do nothing.
  }

  void FakeCloseEcheAppFunction() {
    // Do nothing.
  }

  void SetStatus(FeatureStatus status) {
    fake_feature_status_provider_.SetStatus(status);
  }

  size_t GetNumberOfRecentAppsInteractionHandlers() {
    return fake_phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->recent_app_click_observer_count();
  }

  void RecentAppClicked(
      const phonehub::Notification::AppMetadata& app_metadata) {
    handler_->OnRecentAppClicked(app_metadata);
  }

  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) {
    handler_->HandleNotificationClick(notification_id, app_metadata);
  }

  std::vector<phonehub::Notification::AppMetadata>
  FetchRecentAppMetadataList() {
    return fake_phone_hub_manager_.fake_recent_apps_interaction_handler()
        ->FetchRecentAppMetadataList();
  }

  void StartStreaming() { handler_->OnStartStreaming(); }

  const std::string& get_package_name() { return package_name_; }

  const std::u16string& get_visible_name() { return visible_name_; }

  int64_t get_user_id() { return user_id_; }

  bool waiting_for_streaming_to_show() {
    return handler_->waiting_for_streaming_to_show();
  }

  EcheTray* eche_tray() { return eche_tray_; }

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  std::unique_ptr<EcheRecentAppClickHandler> handler_;
  std::unique_ptr<EcheDisplayStreamHandler> display_stream_handler_;
  std::string package_name_;
  std::u16string visible_name_;
  int64_t user_id_;
  EcheTray* eche_tray_ = nullptr;  // Not owned

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheRecentAppClickHandlerTest, StatusChangeTransitions) {
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(0u, GetNumberOfRecentAppsInteractionHandlers());
  SetStatus(FeatureStatus::kNotEnabledByPhone);
  EXPECT_EQ(1u, GetNumberOfRecentAppsInteractionHandlers());
}

TEST_F(EcheRecentAppClickHandlerTest, LaunchEcheAppFunction) {
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image(),
      /*icon_color=*/absl::nullopt, /*icon_is_monochrome=*/true, user_id);

  RecentAppClicked(fake_app_metadata);

  EXPECT_EQ(fake_app_metadata.package_name, get_package_name());
  EXPECT_EQ(fake_app_metadata.visible_app_name, get_visible_name());
  EXPECT_EQ(fake_app_metadata.user_id, get_user_id());
}

TEST_F(EcheRecentAppClickHandlerTest, HandleNotificationClick) {
  const int64_t notification_id = 1;
  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image(),
      /*icon_color=*/absl::nullopt, /*icon_is_monochrome=*/true, user_id);

  HandleNotificationClick(notification_id, fake_app_metadata);
  std::vector<phonehub::Notification::AppMetadata> app_metadata =
      FetchRecentAppMetadataList();

  EXPECT_EQ(fake_app_metadata.visible_app_name,
            app_metadata[0].visible_app_name);
  EXPECT_EQ(fake_app_metadata.package_name, app_metadata[0].package_name);
  EXPECT_EQ(fake_app_metadata.user_id, app_metadata[0].user_id);
}

TEST_F(EcheRecentAppClickHandlerTest, StartStreaming) {
  EXPECT_FALSE(waiting_for_streaming_to_show());

  const int64_t user_id = 1;
  const char16_t app_visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  auto fake_app_metadata = phonehub::Notification::AppMetadata(
      app_visible_name, package_name, gfx::Image(),
      /*icon_color=*/absl::nullopt, /*icon_is_monochrome=*/true, user_id);
  RecentAppClicked(fake_app_metadata);

  EXPECT_TRUE(waiting_for_streaming_to_show());

  StartStreaming();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(waiting_for_streaming_to_show());
}

}  // namespace eche_app
}  // namespace ash
