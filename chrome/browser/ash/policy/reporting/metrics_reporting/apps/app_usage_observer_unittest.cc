// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer_type.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::StrEq;

namespace reporting {
namespace {

constexpr char kTestAppId[] = "TestApp";
constexpr char kTestAppPublisherId[] = "com.google.test";
constexpr base::TimeDelta kAppUsageCollectionInterval = base::Minutes(5);

// Mock retriever for the `AppPlatformMetrics` component.
class MockAppPlatformMetricsRetriever : public AppPlatformMetricsRetriever {
 public:
  MockAppPlatformMetricsRetriever() : AppPlatformMetricsRetriever(nullptr) {}
  MockAppPlatformMetricsRetriever(const MockAppPlatformMetricsRetriever&) =
      delete;
  MockAppPlatformMetricsRetriever& operator=(
      const MockAppPlatformMetricsRetriever&) = delete;
  ~MockAppPlatformMetricsRetriever() override = default;

  MOCK_METHOD(void,
              GetAppPlatformMetrics,
              (AppPlatformMetricsCallback callback),
              (override));
};

class AppUsageObserverTest : public ::apps::AppPlatformMetricsServiceTestBase {
 protected:
  void SetUp() override {
    ::apps::AppPlatformMetricsServiceTestBase::SetUp();

    // Disable sync so we disable UKM reporting and eliminate noise for testing
    // purposes.
    sync_service()->SetAllowedByEnterprisePolicy(false);

    // Pre-install app so it can be used by tests to simulate usage.
    InstallOneApp(kTestAppId, ::apps::AppType::kArc, kTestAppPublisherId,
                  ::apps::Readiness::kReady, ::apps::InstallSource::kPlayStore);

    // Set up `AppUsageObserver` with relevant test params.
    auto mock_app_platform_metrics_retriever =
        std::make_unique<MockAppPlatformMetricsRetriever>();
    EXPECT_CALL(*mock_app_platform_metrics_retriever, GetAppPlatformMetrics(_))
        .WillOnce([this](AppPlatformMetricsRetriever::AppPlatformMetricsCallback
                             callback) {
          std::move(callback).Run(
              app_platform_metrics_service()->AppPlatformMetrics());
        });
    app_usage_observer_ = AppUsageObserver::CreateForTest(
        profile(), &reporting_settings_,
        std::move(mock_app_platform_metrics_retriever));
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
    EXPECT_THAT(*usage_dict_pref.FindDict(instance_id.ToString())
                     ->FindString(::apps::kUsageTimeAppIdKey),
                StrEq(kTestAppId));
    EXPECT_THAT(*usage_dict_pref.FindDict(instance_id.ToString())
                     ->FindString(::apps::kUsageTimeAppPublisherIdKey),
                StrEq(kTestAppPublisherId));
    EXPECT_THAT(base::ValueToTimeDelta(
                    usage_dict_pref.FindDict(instance_id.ToString())
                        ->Find(::apps::kReportingUsageTimeDurationKey)),
                Eq(expected_usage_time));
  }

  void SetAllowedAppReportingTypes(const std::vector<std::string>& app_types) {
    base::Value::List allowed_app_types;
    for (const auto& app_type : app_types) {
      allowed_app_types.Append(app_type);
    }
    reporting_settings_.SetList(::ash::reporting::kReportAppUsage,
                                std::move(allowed_app_types));
  }

  // Fake reporting settings component used by the test.
  test::FakeReportingSettings reporting_settings_;

  // App usage observer used by tests.
  std::unique_ptr<AppUsageObserver> app_usage_observer_;
};

TEST_F(AppUsageObserverTest, PersistAppUsageDataInPrefStore) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify data is persisted in the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);
}

TEST_F(AppUsageObserverTest, ShouldNotPersistMicrosecondUsageData) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});

  // Create a new window for the app and simulate insignificant app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Microseconds(200);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify data is not persisted in the pref store.
  ASSERT_TRUE(GetPrefService()->GetDict(::apps::kAppUsageTime).empty());
}

TEST_F(AppUsageObserverTest, ShouldNotPersistAppUsageDataIfSettingUnset) {
  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify there is no data persisted to the pref store.
  const auto& usage_dict_pref =
      GetPrefService()->GetDict(::apps::kAppUsageTime);
  ASSERT_TRUE(usage_dict_pref.empty());
}

TEST_F(AppUsageObserverTest, ShouldNotPersistAppUsageDataIfAppTypeDisallowed) {
  // Set policy to enable reporting for a different app type than the one being
  // used.
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryPWA});

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify there is no data persisted to the pref store.
  const auto& usage_dict_pref =
      GetPrefService()->GetDict(::apps::kAppUsageTime);
  ASSERT_TRUE(usage_dict_pref.empty());
}

TEST_F(AppUsageObserverTest, ShouldAggregateAppUsageDataOnSubsequentUsage) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify data is persisted to the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Simulate additional app usage.
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify aggregated usage data is persisted in the pref store.
  const auto& expected_usage_duration = kAppUsageDuration + kAppUsageDuration;
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId,
                                           expected_usage_duration);
}

TEST_F(AppUsageObserverTest,
       ShouldStopPersistingAppUsageDataWhenSettingDisabled) {
  // Allow reporting for specified app type initially.
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify data is persisted to the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Disallow reporting and simulate additional app usage.
  SetAllowedAppReportingTypes({});
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify usage data remains unchanged in the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);
}

TEST_F(AppUsageObserverTest, ShouldPersistAppUsageDataForNewInstance) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});

  // Create a new window for the app and simulate app usage.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify data is persisted to the pref store.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Simulate app usage for a new instance of the same app.
  ::aura::Window window1(nullptr);
  window1.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId1 = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window1, kInstanceId1, kAppUsageDuration);

  // Verify the component persists usage data for the new instance.
  ASSERT_THAT(GetPrefService()->GetDict(::apps::kAppUsageTime).size(), Eq(2UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId1, kAppUsageDuration);
}

TEST_F(AppUsageObserverTest, OnAppPlatformMetricsDestroyed) {
  SetAllowedAppReportingTypes({::ash::reporting::kAppCategoryAndroidApps});

  // Reset `AppPlatformMetricsService` to destroy the `AppPlatformMetrics`
  // component.
  ResetAppPlatformMetricsService();

  // Create a new window for the app and simulate app usage to verify the
  // component is no longer tracking app usage metric.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  ::aura::Window window(nullptr);
  window.Init(::ui::LAYER_NOT_DRAWN);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  SimulateAppUsageForInstance(&window, kInstanceId, kAppUsageDuration);

  // Verify there is no data persisted to the pref store.
  const auto& usage_dict_pref =
      GetPrefService()->GetDict(::apps::kAppUsageTime);
  ASSERT_TRUE(usage_dict_pref.empty());
}

}  // namespace
}  // namespace reporting
