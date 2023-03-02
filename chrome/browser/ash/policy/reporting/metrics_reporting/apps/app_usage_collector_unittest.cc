// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_collector.h"

#include <memory>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_type.h"

using ::testing::Eq;
using ::testing::NotNull;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestAppId[] = "TestApp";
constexpr base::TimeDelta kAppUsageCollectionInterval = base::Minutes(5);

class AppUsageCollectorTest : public ::apps::AppPlatformMetricsServiceTestBase {
 public:
  void SetUp() override {
    ::apps::AppPlatformMetricsServiceTestBase::SetUp();

    // Disable sync so we disable UKM reporting and eliminate noise for testing
    // purposes.
    sync_service()->SetDisableReasons(
        syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);

    // Pre-install app so it can be used by tests to simulate usage.
    InstallOneApp(kTestAppId, ::apps::AppType::kArc, /*publisher_id=*/"",
                  ::apps::Readiness::kReady, ::apps::InstallSource::kPlayStore);
  }

  void SimulateAppUsageForInstance(::aura::Window* window,
                                   const base::UnguessableToken& instance_id,
                                   const base::TimeDelta& usage_duration) {
    // Set the window active state and simulate app usage by advancing timer.
    ModifyInstance(instance_id, kTestAppId, window,
                   ::apps::InstanceState::kActive);
    task_environment_.FastForwardBy(usage_duration);

    // Set app inactive by closing window.
    ModifyInstance(instance_id, kTestAppId, window,
                   ::apps::InstanceState::kDestroyed);

    // Advance timer by the collection interval to trigger metric collection.
    task_environment_.FastForwardBy(kAppUsageCollectionInterval);
  }

  void VerifyAppUsageDataInPrefStoreForInstance(
      const base::UnguessableToken& instance_id,
      const base::TimeDelta& expected_usage_time) {
    const auto& usage_dict_pref =
        GetPrefService()->GetDict(::apps::kAppUsageTime);
    ASSERT_THAT(usage_dict_pref.Find(instance_id.ToString()), NotNull());
    EXPECT_THAT(*usage_dict_pref.Find(instance_id.ToString())
                     ->FindStringKey(::apps::kUsageTimeAppIdKey),
                StrEq(kTestAppId));
    EXPECT_THAT(base::ValueToTimeDelta(
                    usage_dict_pref.FindDict(instance_id.ToString())
                        ->Find(::apps::kReportingUsageTimeDurationKey)),
                Eq(expected_usage_time));
  }

 protected:
  test::FakeReportingSettings reporting_settings_;
};

TEST_F(AppUsageCollectorTest, PersistAppUsageDataInPrefStore) {
  reporting_settings_.SetBoolean(::ash::kReportDeviceAppInfo, true);
  AppUsageCollector app_usage_collector(
      profile(), &reporting_settings_,
      app_platform_metrics_service()->AppPlatformMetrics());

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const auto window = std::make_unique<::aura::Window>(nullptr);
  window->Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify data is persisted in the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);
}

TEST_F(AppUsageCollectorTest, ShouldNotPersistAppUsageDataIfSettingDisabled) {
  reporting_settings_.SetBoolean(::ash::kReportDeviceAppInfo, false);
  AppUsageCollector app_usage_collector(
      profile(), &reporting_settings_,
      app_platform_metrics_service()->AppPlatformMetrics());

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const auto window = std::make_unique<::aura::Window>(nullptr);
  window->Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify there is no data persisted to the pref store.
  const auto& usage_dict_pref =
      GetPrefService()->GetDict(::apps::kAppUsageTime);
  ASSERT_TRUE(usage_dict_pref.empty());
}

TEST_F(AppUsageCollectorTest, ShouldAggregateAppUsageDataOnSubsequentUsage) {
  reporting_settings_.SetBoolean(::ash::kReportDeviceAppInfo, true);
  AppUsageCollector app_usage_collector(
      profile(), &reporting_settings_,
      app_platform_metrics_service()->AppPlatformMetrics());

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const auto window = std::make_unique<::aura::Window>(nullptr);
  window->Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify data is persisted to the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Simulate additional app usage.
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify aggregated usage data is persisted in the pref store.
  const auto& expected_usage_duration = kAppUsageDuration + kAppUsageDuration;
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId,
                                           expected_usage_duration);
}

TEST_F(AppUsageCollectorTest,
       ShouldStopPersistingAppUsageDataWhenSettingDisabled) {
  // Enable reporting setting initially.
  reporting_settings_.SetBoolean(::ash::kReportDeviceAppInfo, true);
  AppUsageCollector app_usage_collector(
      profile(), &reporting_settings_,
      app_platform_metrics_service()->AppPlatformMetrics());

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const auto window = std::make_unique<::aura::Window>(nullptr);
  window->Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify data is persisted to the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Disable setting and simulate additional app usage.
  reporting_settings_.SetBoolean(::ash::kReportDeviceAppInfo, false);
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify usage data remains unchanged in the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);
}

TEST_F(AppUsageCollectorTest, ShouldPersistAppUsageDataForNewInstance) {
  reporting_settings_.SetBoolean(::ash::kReportDeviceAppInfo, true);
  AppUsageCollector app_usage_collector(
      profile(), &reporting_settings_,
      app_platform_metrics_service()->AppPlatformMetrics());

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const auto window = std::make_unique<::aura::Window>(nullptr);
  window->Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(window.get(), kInstanceId, kAppUsageDuration);

  // Verify data is persisted to the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Simulate app usage for a new instance of the same app.
  const auto window1 = std::make_unique<::aura::Window>(nullptr);
  window1->Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId1 = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(window1.get(), kInstanceId1, kAppUsageDuration);

  // Verify the component persists usage data for the new instance.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(2UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId1, kAppUsageDuration);
}

}  // namespace
}  // namespace reporting
