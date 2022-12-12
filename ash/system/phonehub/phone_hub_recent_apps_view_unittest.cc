// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_apps_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/phonehub/phone_hub_recent_app_button.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

namespace {

using FeatureState = multidevice_setup::mojom::FeatureState;

const char16_t kAppName[] = u"Test App";
const char kPackageName[] = "com.google.testapp";
const int64_t kUserId = 0;

}  // namespace

class RecentAppButtonsViewTest : public AshTestBase {
 public:
  RecentAppButtonsViewTest() = default;
  ~RecentAppButtonsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA},
        /*disabled_features=*/{});

    phone_hub_recent_apps_view_ = std::make_unique<PhoneHubRecentAppsView>(
        &fake_recent_apps_interaction_handler_, &fake_phone_hub_manager_);
  }

  void TearDown() override {
    phone_hub_recent_apps_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  PhoneHubRecentAppsView* recent_apps_view() {
    return phone_hub_recent_apps_view_.get();
  }

  void NotifyRecentAppAddedOrUpdated() {
    fake_recent_apps_interaction_handler_.NotifyRecentAppAddedOrUpdated(
        phonehub::Notification::AppMetadata(
            kAppName, kPackageName,
            /*icon=*/gfx::Image(), /*icon_color =*/absl::nullopt,
            /*icon_is_monochrome =*/true, kUserId,
            phonehub::proto::AppStreamabilityStatus::STREAMABLE),
        base::Time::Now());
  }

  size_t PackageNameToClickCount(const std::string& package_name) {
    return fake_recent_apps_interaction_handler_.HandledRecentAppsCount(
        package_name);
  }

  void FeatureStateChanged(FeatureState feature_state) {
    fake_recent_apps_interaction_handler_.OnFeatureStateChanged(feature_state);
  }

  bool AppStreamLauncherShowState() {
    return fake_phone_hub_manager_.fake_app_stream_launcher_data_model()
        ->GetShouldShowMiniLauncher();
  }

 private:
  std::unique_ptr<PhoneHubRecentAppsView> phone_hub_recent_apps_view_;
  phonehub::FakeRecentAppsInteractionHandler
      fake_recent_apps_interaction_handler_;
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(RecentAppButtonsViewTest, TaskViewVisibility) {
  // The recent app view is not visible if the NotifyRecentAppAddedOrUpdated
  // function never be called, e.g. device boot.
  EXPECT_FALSE(recent_apps_view()->GetVisible());

  // The feature state is enabled but no recent app has been added yet, we
  // should not show the recent app buttons view.
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(recent_apps_view()->recent_app_buttons_view_->GetVisible());

  // The feature state is disabled so we should not show all recent apps view.
  FeatureStateChanged(FeatureState::kDisabledByUser);
  recent_apps_view()->Update();

  EXPECT_FALSE(recent_apps_view()->GetVisible());
}

TEST_F(RecentAppButtonsViewTest, SingleRecentAppButtonsView) {
  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  size_t expected_recent_app_button = 1;
  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());
}

TEST_F(RecentAppButtonsViewTest, MultipleRecentAppButtonsView) {
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  size_t expected_recent_app_button = 3;
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());

  for (auto* child : recent_apps_view()->recent_app_buttons_view_->children()) {
    PhoneHubRecentAppButton* recent_app =
        static_cast<PhoneHubRecentAppButton*>(child);
    // Simulate clicking button using placeholder event.
    views::test::ButtonTestApi(recent_app).NotifyClick(ui::test::TestEvent());
  }

  size_t expected_number_of_button_be_clicked = 3;
  EXPECT_EQ(expected_number_of_button_be_clicked,
            PackageNameToClickCount(kPackageName));
}

TEST_F(RecentAppButtonsViewTest,
       MultipleRecentAppButtonsWithMoreAppsButtonView) {
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  size_t expected_recent_app_button = 6;
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());

  for (std::size_t i = 0;
       i != recent_apps_view()->recent_app_buttons_view_->children().size();
       i++) {
    auto* child = recent_apps_view()->recent_app_buttons_view_->children()[i];
    if (i == 5) {
      views::ImageButton* more_apps_button =
          static_cast<views::ImageButton*>(child);
      views::test::ButtonTestApi(more_apps_button)
          .NotifyClick(ui::test::TestEvent());
      break;
    }
    PhoneHubRecentAppButton* recent_app =
        static_cast<PhoneHubRecentAppButton*>(child);
    // Simulate clicking button using placeholder event.
    views::test::ButtonTestApi(recent_app).NotifyClick(ui::test::TestEvent());
  }

  size_t expected_number_of_button_be_clicked = 5;
  EXPECT_EQ(expected_number_of_button_be_clicked,
            PackageNameToClickCount(kPackageName));
  EXPECT_TRUE(AppStreamLauncherShowState());
}

}  // namespace ash
