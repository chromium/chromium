// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/website_telemetry_reporting_nudge_controller.h"

#include <memory>
#include <string>

#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/test/ash_test_base.h"
#include "base/test/task_environment.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/account_id/account_id.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::session_manager::SessionState;

namespace reporting {
namespace {

class WebsiteTelemetryReportingNudgeControllerTest : public ::ash::AshTestBase {
 protected:
  WebsiteTelemetryReportingNudgeControllerTest()
      : ::ash::AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}

  void SetUp() override {
    ::ash::AshTestBase::SetUp();

    // Set up test profile.
    TestingProfile::Builder profile_builder;
    profile_builder.SetProfileName("test@test.com");
    profile_ = profile_builder.Build();

    // Create user session and register session state to be active by default.
    const auto account_id =
        AccountId::FromUserEmail(profile_->GetProfileUserName());
    GetSessionControllerClient()->AddUserSession(
        account_id.GetUserEmail(), ::user_manager::USER_TYPE_REGULAR,
        /*provide_pref_service=*/true, /*is_new_profile=*/true);
    SetSessionState(SessionState::ACTIVE);
  }

  void SetSessionState(SessionState session_state) {
    auto* const session_manager = ::session_manager::SessionManager::Get();
    CHECK(session_manager);
    session_manager->SetSessionState(session_state);
  }

  bool IsNudgeShown() {
    auto* const anchor_nudge_manager = ::ash::AnchoredNudgeManager::Get();
    CHECK(anchor_nudge_manager);
    return anchor_nudge_manager->IsNudgeShown(
        kWebsiteTelemetryReportingNudgeId);
  }

  void CancelNudge() {
    auto* const anchor_nudge_manager = ::ash::AnchoredNudgeManager::Get();
    CHECK(anchor_nudge_manager);
    anchor_nudge_manager->Cancel(kWebsiteTelemetryReportingNudgeId);
  }

  test::FakeReportingSettings reporting_settings_;
  std::unique_ptr<TestingProfile> profile_;
};

TEST_F(WebsiteTelemetryReportingNudgeControllerTest, OnInit_SettingEnabled) {
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, true);
  ASSERT_FALSE(IsNudgeShown());

  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  EXPECT_TRUE(IsNudgeShown());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

TEST_F(WebsiteTelemetryReportingNudgeControllerTest, OnInit_SettingDisabled) {
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, false);
  ASSERT_FALSE(IsNudgeShown());

  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  EXPECT_FALSE(IsNudgeShown());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

TEST_F(WebsiteTelemetryReportingNudgeControllerTest, OnInit_SettingUnset) {
  ASSERT_FALSE(IsNudgeShown());
  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  EXPECT_FALSE(IsNudgeShown());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

TEST_F(WebsiteTelemetryReportingNudgeControllerTest,
       DelayNotificationUntilSessionActive) {
  SetSessionState(SessionState::LOGGED_IN_NOT_ACTIVE);
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, true);
  ASSERT_FALSE(IsNudgeShown());

  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  ASSERT_FALSE(IsNudgeShown());
  SetSessionState(SessionState::ACTIVE);
  EXPECT_TRUE(IsNudgeShown());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

TEST_F(WebsiteTelemetryReportingNudgeControllerTest, OnSettingFirstEnabled) {
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, false);
  ASSERT_FALSE(IsNudgeShown());

  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  ASSERT_FALSE(IsNudgeShown());
  ASSERT_FALSE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));

  // Set website reporting and verify nudge shown.
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, true);
  EXPECT_TRUE(IsNudgeShown());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

TEST_F(WebsiteTelemetryReportingNudgeControllerTest, OnSettingFirstDisabled) {
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, true);
  ASSERT_FALSE(IsNudgeShown());

  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  ASSERT_TRUE(IsNudgeShown());
  ASSERT_TRUE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
  CancelNudge();

  // Unset website reporting and verify nudge is not shown.
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, false);
  EXPECT_FALSE(IsNudgeShown());
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

TEST_F(WebsiteTelemetryReportingNudgeControllerTest,
       ShowNotificationOnInitOnlyOnce) {
  reporting_settings_.SetReportingEnabled(kReportWebsiteTelemetry, true);
  ASSERT_FALSE(IsNudgeShown());

  {
    WebsiteTelemetryReportingNudgeController nudge_controller(
        profile_->GetWeakPtr(), &reporting_settings_);
    ASSERT_TRUE(IsNudgeShown());
    ASSERT_TRUE(profile_->GetPrefs()->GetBoolean(
        kReportWebsiteTelemetryPreviouslyEnabled));
    CancelNudge();
  }

  // Nudge should not be shown on init since it has already been displayed once
  // previously.
  WebsiteTelemetryReportingNudgeController nudge_controller(
      profile_->GetWeakPtr(), &reporting_settings_);
  EXPECT_FALSE(IsNudgeShown());
  EXPECT_TRUE(profile_->GetPrefs()->GetBoolean(
      kReportWebsiteTelemetryPreviouslyEnabled));
}

}  // namespace
}  // namespace reporting
