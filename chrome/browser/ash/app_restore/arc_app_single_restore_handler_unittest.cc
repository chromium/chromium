// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_app_single_restore_handler.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_restore/arc_ghost_window_handler.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/exo/wm_helper.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::app_restore {

namespace {

constexpr char kTestProfileName[] = "user@gmail.com";

class FakeArcGhostWindowHandler : public full_restore::ArcGhostWindowHandler {
 public:
  bool LaunchArcGhostWindow(
      const std::string& app_id,
      int32_t session_id,
      ::app_restore::AppRestoreData* restore_data) override {
    return true;
  }
};

}  // namespace

class ArcAppSingleRestoreHandlerTest : public testing::Test {
 public:
  ArcAppSingleRestoreHandlerTest() = default;
  ArcAppSingleRestoreHandlerTest(const ArcAppSingleRestoreHandlerTest&) =
      delete;
  ArcAppSingleRestoreHandlerTest& operator=(
      const ArcAppSingleRestoreHandlerTest&) = delete;
  ~ArcAppSingleRestoreHandlerTest() override = default;

  void SetUp() override {
    test_user_session_manager_ =
        std::make_unique<ash::test::TestUserSessionManager>(
            TestingBrowserProcess::GetGlobal()->local_state());

    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());

    wm_helper_ = std::make_unique<exo::WMHelper>();
    ghost_window_handler_ = std::make_unique<FakeArcGhostWindowHandler>();

    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestProfileName, GaiaId("12345678"));
    ASSERT_TRUE(test_user_session_manager_->AddRegularUser(account_id));
    test_user_session_manager_->LogIn(account_id);

    profile_ = profile_manager_->CreateTestingProfile(kTestProfileName);
  }

  void TearDown() override {
    profile_ = nullptr;
    ghost_window_handler_.reset();
    wm_helper_.reset();
    profile_manager_.reset();
    test_user_session_manager_.reset();
  }

  TestingProfile* profile() const { return profile_; }

  full_restore::ArcGhostWindowHandler* window_handler() {
    return ghost_window_handler_.get();
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ash::test::TestUserSessionManager> test_user_session_manager_;
  std::unique_ptr<TestingProfileManager> profile_manager_;

  // Initialize WMHelper to create ARC ghost window handler.
  std::unique_ptr<exo::WMHelper> wm_helper_;
  std::unique_ptr<FakeArcGhostWindowHandler> ghost_window_handler_;

  raw_ptr<TestingProfile> profile_ = nullptr;
};

TEST_F(ArcAppSingleRestoreHandlerTest, NotLaunchIfShelfNotReady) {
  ArcAppSingleRestoreHandler handler;
  handler.LaunchGhostWindowWithApp(
      profile(), "not_exist_app_id", nullptr, 0 /*event_flags*/,
      arc::GhostWindowType::kAppLaunch, arc::mojom::WindowInfoPtr());
  ASSERT_FALSE(handler.app_id_.has_value());
}

TEST_F(ArcAppSingleRestoreHandlerTest, PendingLaunchIfShelfHasReady) {
  ArcAppSingleRestoreHandler handler;

  const std::string fake_app_id = "not_exist_app_id";
  auto window_info = arc::mojom::WindowInfo::New();
  window_info->window_id = 100;
  window_info->bounds = gfx::Rect(100, 100, 800, 800);
  window_info->display_id = display::kInvalidDisplayId;

  handler.OnShelfReady();
  handler.ghost_window_handler_ = window_handler();
  handler.LaunchGhostWindowWithApp(
      profile(), fake_app_id, nullptr, 0 /*event_flags*/,
      arc::GhostWindowType::kAppLaunch, std::move(window_info));
  ASSERT_TRUE(handler.app_id_.has_value());
  ASSERT_TRUE(handler.IsAppPendingRestore(fake_app_id));
  ASSERT_FALSE(handler.IsAppPendingRestore(fake_app_id + "_not_equal_real_id"));
}

TEST_F(ArcAppSingleRestoreHandlerTest, NullBoundsNotCauseCrash) {
  ArcAppSingleRestoreHandler handler;

  const std::string fake_app_id = "not_exist_app_id";
  auto window_info = arc::mojom::WindowInfo::New();
  window_info->window_id = 100;
  window_info->display_id = display::kInvalidDisplayId;
  // leave the bounds null.

  handler.OnShelfReady();
  handler.ghost_window_handler_ = window_handler();
  handler.LaunchGhostWindowWithApp(
      profile(), fake_app_id, nullptr, 0 /*event_flags*/,
      arc::GhostWindowType::kAppLaunch, std::move(window_info));
}

}  // namespace ash::app_restore
