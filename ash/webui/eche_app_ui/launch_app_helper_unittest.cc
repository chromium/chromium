// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/launch_app_helper.h"

#include <ostream>
#include <string>

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/components/phonehub/screen_lock_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_suite.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

namespace {
void CloseEcheAppFunction() {}

void LaunchEcheAppFunction(const absl::optional<int64_t>& notification_id,
                           const std::string& package_name,
                           const std::u16string& visible_name,
                           const absl::optional<int64_t>& user_id,
                           const gfx::Image& icon) {}

void LaunchNotificationFunction(
    const absl::optional<std::u16string>& title,
    const absl::optional<std::u16string>& message,
    std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {}
}  // namespace

class LaunchAppHelperTest : public ash::AshTestBase {
 protected:
  LaunchAppHelperTest() = default;
  LaunchAppHelperTest(const LaunchAppHelperTest&) = delete;
  LaunchAppHelperTest& operator=(const LaunchAppHelperTest&) = delete;
  ~LaunchAppHelperTest() override = default;

  // ash::AshTestBase:
  void SetUp() override {
    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{chromeos::features::kEcheSWA},
        /*disabled_features=*/{});

    fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
    launch_app_helper_ = std::make_unique<LaunchAppHelper>(
        fake_phone_hub_manager_.get(),
        base::BindRepeating(&LaunchEcheAppFunction),
        base::BindRepeating(&CloseEcheAppFunction),
        base::BindRepeating(&LaunchNotificationFunction));
    toast_manager_ = Shell::Get()->toast_manager();
  }

  void TearDown() override { AshTestBase::TearDown(); }

  LaunchAppHelper::AppLaunchProhibitedReason ProhibitedByPolicy(
      FeatureStatus status) const {
    return launch_app_helper_->CheckAppLaunchProhibitedReason(status);
  }

  void SetLockStatus(phonehub::ScreenLockManager::LockStatus lock_status) {
    fake_phone_hub_manager_->fake_screen_lock_manager()->SetLockStatusInternal(
        lock_status);
  }

  void ShowToast(const std::u16string& text) {
    launch_app_helper_->ShowToast(text);
  }

  void VerifyShowToast(const std::u16string& text) {
    ToastOverlay* overlay = toast_manager_->GetCurrentOverlayForTesting();
    ASSERT_NE(nullptr, overlay);
    EXPECT_EQ(overlay->GetText(), text);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<phonehub::FakePhoneHubManager> fake_phone_hub_manager_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  ToastManagerImpl* toast_manager_ = nullptr;
};

TEST_F(LaunchAppHelperTest, TestProhibitedByPolicy) {
  SetCanLockScreen(true);
  SetShouldLockScreenAutomatically(true);
  SetLockStatus(phonehub::ScreenLockManager::LockStatus::kLockedOn);

  constexpr FeatureStatus kConvertableStatus[] = {
      FeatureStatus::kIneligible,       FeatureStatus::kDisabled,
      FeatureStatus::kConnecting,       FeatureStatus::kConnected,
      FeatureStatus::kDependentFeature, FeatureStatus::kDependentFeaturePending,
  };

  for (const auto status : kConvertableStatus) {
    EXPECT_EQ(LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited,
              ProhibitedByPolicy(status));
  }

  // The screenLock is required.
  SetCanLockScreen(false);
  SetShouldLockScreenAutomatically(false);

  for (const auto status : kConvertableStatus) {
    EXPECT_EQ(LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock,
              ProhibitedByPolicy(status));
  }
}

TEST_F(LaunchAppHelperTest, VerifyShowToast) {
  const std::u16string text = u"text";

  ShowToast(text);

  VerifyShowToast(text);
}

}  // namespace eche_app
}  // namespace ash
