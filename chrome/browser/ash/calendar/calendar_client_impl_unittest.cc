// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/calendar/calendar_client_impl.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/glanceables/glanceables_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CalendarClientImplTest : public testing::Test {
 public:
  CalendarClientImplTest()
      : profile_manager_(
            TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override { ASSERT_TRUE(profile_manager_.SetUp()); }

  std::unique_ptr<sync_preferences::TestingPrefServiceSyncable>
  GetDefaultPrefs() const {
    auto prefs =
        std::make_unique<sync_preferences::TestingPrefServiceSyncable>();
    RegisterUserProfilePrefs(prefs->registry());
    return prefs;
  }

  TestingProfile* CreateTestingProfile(
      std::unique_ptr<sync_preferences::TestingPrefServiceSyncable> prefs) {
    return profile_manager_.CreateTestingProfile(
        "profile@example.com", std::move(prefs), u"User Name", /*avatar_id=*/0,
        TestingProfile::TestingFactories());
  }

  const base::HistogramTester* histogram_tester() const {
    return &histogram_tester_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  const base::HistogramTester histogram_tester_;
  TestingProfileManager profile_manager_;
};

TEST_F(CalendarClientImplTest, IsDisabledByAdmin_Default) {
  auto* const profile = CreateTestingProfile(GetDefaultPrefs());

  const auto client = CalendarClientImpl(profile);
  EXPECT_FALSE(client.IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleCalendar.Status",
      ContextualGoogleIntegrationStatus::kEnabled,
      /*expected_bucket_count=*/1);
}

TEST_F(CalendarClientImplTest, IsDisabledByAdmin_DisabledCalendarPref) {
  auto prefs = GetDefaultPrefs();
  prefs->SetBoolean(prefs::kCalendarIntegrationEnabled, false);

  auto* const profile = CreateTestingProfile(std::move(prefs));

  const auto client = CalendarClientImpl(profile);
  EXPECT_TRUE(client.IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleCalendar.Status",
      ContextualGoogleIntegrationStatus::kDisabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(CalendarClientImplTest,
       IsDisabledByAdmin_NoCalendarInContextualGoogleIntegrationsPref) {
  auto prefs = GetDefaultPrefs();
  base::Value::List enabled_integrations;
  enabled_integrations.Append(prefs::kGoogleClassroomIntegrationName);
  enabled_integrations.Append(prefs::kGoogleTasksIntegrationName);
  prefs->SetList(prefs::kContextualGoogleIntegrationsConfiguration,
                 std::move(enabled_integrations));

  auto* const profile = CreateTestingProfile(std::move(prefs));

  const auto client = CalendarClientImpl(profile);
  EXPECT_TRUE(client.IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleCalendar.Status",
      ContextualGoogleIntegrationStatus::kDisabledByPolicy,
      /*expected_bucket_count=*/1);
}

TEST_F(CalendarClientImplTest, IsDisabledByAdmin_DisabledCalendarApp) {
  auto* const profile = CreateTestingProfile(GetDefaultPrefs());

  std::vector<apps::AppPtr> app_deltas;
  app_deltas.push_back(apps::AppPublisher::MakeApp(
      apps::AppType::kWeb, web_app::kGoogleCalendarAppId,
      apps::Readiness::kDisabledByPolicy, "Calendar",
      apps::InstallReason::kUser, apps::InstallSource::kBrowser));
  apps::AppServiceProxyFactory::GetForProfile(profile)->OnApps(
      std::move(app_deltas), apps::AppType::kWeb,
      /*should_notify_initialized=*/true);

  const auto client = CalendarClientImpl(profile);
  EXPECT_TRUE(client.IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleCalendar.Status",
      ContextualGoogleIntegrationStatus::kDisabledByAppBlock,
      /*expected_bucket_count=*/1);
}

TEST_F(CalendarClientImplTest, IsDisabledByAdmin_BlockedCalendarUrl) {
  auto prefs = GetDefaultPrefs();
  base::Value::List blocklist;
  blocklist.Append("calendar.google.com");
  prefs->SetManagedPref(policy::policy_prefs::kUrlBlocklist,
                        std::move(blocklist));

  auto* const profile = CreateTestingProfile(std::move(prefs));

  const auto client = CalendarClientImpl(profile);
  EXPECT_TRUE(client.IsDisabledByAdmin());
  histogram_tester()->ExpectUniqueSample(
      "Ash.ContextualGoogleIntegrations.GoogleCalendar.Status",
      ContextualGoogleIntegrationStatus::kDisabledByUrlBlock,
      /*expected_bucket_count=*/1);
}

}  // namespace ash
