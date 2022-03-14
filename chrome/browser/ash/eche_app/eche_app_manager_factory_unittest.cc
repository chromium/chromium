// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"

#include "ash/constants/ash_features.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace ash {
namespace eche_app {

class EcheAppManagerFactoryTest : public ChromeAshTestBase {
 protected:
  EcheAppManagerFactoryTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA, features::kEcheCustomWidget},
        /*disabled_features=*/{features::kEcheSWAInBackground});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    if (profile_manager_->SetUp()) {
      profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    }
  }
  ~EcheAppManagerFactoryTest() override = default;
  EcheAppManagerFactoryTest(const EcheAppManagerFactoryTest&) = delete;
  EcheAppManagerFactoryTest& operator=(const EcheAppManagerFactoryTest&) =
      delete;

  // AshTestBase::Test:
  void SetUp() override {
    DCHECK(profile_);
    DCHECK(test_web_view_factory_.get());
    ChromeAshTestBase::SetUp();
    eche_tray_ =
        ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
  }

  TestingProfile* GetProfile() { return profile_; }

  EcheTray* eche_tray() { return eche_tray_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  EcheTray* eche_tray_ = nullptr;
  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

class EcheAppManagerFactoryWithBackgroundTest : public ChromeAshTestBase {
 protected:
  EcheAppManagerFactoryWithBackgroundTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA, features::kEcheCustomWidget,
                              features::kEcheSWAInBackground},
        /*disabled_features=*/{});
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    if (profile_manager_->SetUp()) {
      profile_ = profile_manager_->CreateTestingProfile("testing_profile");
    }
  }
  ~EcheAppManagerFactoryWithBackgroundTest() override = default;
  EcheAppManagerFactoryWithBackgroundTest(
      const EcheAppManagerFactoryWithBackgroundTest&) = delete;
  EcheAppManagerFactoryWithBackgroundTest& operator=(
      const EcheAppManagerFactoryWithBackgroundTest&) = delete;

  // AshTestBase::Test:
  void SetUp() override {
    DCHECK(profile_);
    DCHECK(test_web_view_factory_.get());
    ChromeAshTestBase::SetUp();
    eche_tray_ =
        ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
  }

  TestingProfile* GetProfile() { return profile_; }

  EcheTray* eche_tray() { return eche_tray_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  EcheTray* eche_tray_ = nullptr;
  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheAppManagerFactoryTest, LaunchEcheApp) {
  const int64_t user_id = 1;
  const char16_t visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/absl::nullopt, package_name,
      visible_name, user_id, gfx::Image());
  base::RunLoop().RunUntilIdle();
  // Eche tray should be visible after launch.
  EXPECT_TRUE(eche_tray()->is_active());
}

TEST_F(EcheAppManagerFactoryTest, CloseEche) {
  const int64_t user_id = 1;
  const char16_t visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/absl::nullopt, package_name,
      visible_name, user_id, gfx::Image());
  EcheAppManagerFactory::CloseEche(GetProfile());
  base::RunLoop().RunUntilIdle();
  // Eche tray should be visible after close.
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheAppManagerFactoryTest, OnStreamStateChanged) {
  const int64_t user_id = 1;
  const char16_t visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/absl::nullopt, package_name,
      visible_name, user_id, gfx::Image());

  // Eche tray should be visible when streaming is active
  EcheAppManagerFactory::OnStreamStateChanged(
      GetProfile(), mojom::StreamStatus::kStreamStatusStarted);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());

  // Eche tray should not be visible when streaming is finished
  EcheAppManagerFactory::OnStreamStateChanged(
      GetProfile(), mojom::StreamStatus::kStreamStatusStopped);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheAppManagerFactoryWithBackgroundTest, LaunchEcheApp) {
  const int64_t user_id = 1;
  const char16_t visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/absl::nullopt, package_name,
      visible_name, user_id, gfx::Image());
  base::RunLoop().RunUntilIdle();
  // Eche tray should be visible when streaming is active, not ative when
  // launch.
  EXPECT_FALSE(eche_tray()->is_active());
}

TEST_F(EcheAppManagerFactoryWithBackgroundTest, OnStreamStateChanged) {
  const int64_t user_id = 1;
  const char16_t visible_name[] = u"Fake App";
  const char package_name[] = "com.fakeapp";
  EcheAppManagerFactory::LaunchEcheApp(
      GetProfile(), /*notification_id=*/absl::nullopt, package_name,
      visible_name, user_id, gfx::Image());

  // Eche tray should be visible when streaming is active
  EcheAppManagerFactory::OnStreamStateChanged(
      GetProfile(), mojom::StreamStatus::kStreamStatusStarted);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(eche_tray()->is_active());

  // Eche tray should not be visible when streaming is finished
  EcheAppManagerFactory::OnStreamStateChanged(
      GetProfile(), mojom::StreamStatus::kStreamStatusStopped);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(eche_tray()->is_active());
}

}  // namespace eche_app
}  // namespace ash
