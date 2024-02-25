// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/apps/app_usage_telemetry_sampler.h"

#include <memory>
#include <optional>
#include <string>
#include <tuple>

#include "base/json/values_util.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/protos/app_types.pb.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

namespace reporting {
namespace {

constexpr char kTestUserEmail[] = "test@test.com";
constexpr char kTestAppId[] = "TestApp";
constexpr char kTestAppPublisherId[] = "com.google.test";

class AppUsageTelemetrySamplerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set up user manager and test profile.
    fake_user_manager_ = new ::ash::FakeChromeUserManager();
    scoped_user_manager_ = std::make_unique<::user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_.get()));
    AccountId account_id = AccountId::FromUserEmail(kTestUserEmail);
    const ::user_manager::User* const user =
        fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);
    profile_ = std::make_unique<TestingProfile>();
    ::ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, profile_.get());

    // Set up app usage telemetry sampler for the test profile.
    app_usage_telemetry_sampler_ =
        std::make_unique<AppUsageTelemetrySampler>(profile_->GetWeakPtr());
  }

  // Simulates app usage for the specified app usage duration by aggregating
  // relevant usage info in the pref store.
  void CreateOrUpdateAppUsageForInstance(
      const base::UnguessableToken& instance_id,
      const base::TimeDelta& usage_duration) {
    PrefService* const user_prefs = profile_->GetPrefs();
    if (!user_prefs->HasPrefPath(::apps::kAppUsageTime)) {
      // Create empty dictionary if none exists in the pref store.
      user_prefs->SetDict(::apps::kAppUsageTime, base::Value::Dict());
    }

    ScopedDictPrefUpdate usage_dict_pref(profile_->GetPrefs(),
                                         ::apps::kAppUsageTime);
    const auto& instance_id_string = instance_id.ToString();
    if (!usage_dict_pref->contains(instance_id_string)) {
      // Create a new entry in the pref store with the specified running time.
      ::apps::AppPlatformMetrics::UsageTime usage_time;
      usage_time.app_id = kTestAppId;
      usage_time.app_publisher_id = kTestAppPublisherId;
      usage_time.reporting_usage_time = usage_duration;
      usage_dict_pref->SetByDottedPath(instance_id_string,
                                       usage_time.ConvertToDict());
      return;
    }

    // Aggregate and update just the running time otherwise.
    ::apps::AppPlatformMetrics::UsageTime usage_time(
        *usage_dict_pref->FindByDottedPath(instance_id_string));
    usage_time.reporting_usage_time += usage_duration;
    usage_dict_pref->SetByDottedPath(instance_id_string,
                                     usage_time.ConvertToDict());
  }

  void VerifyAppUsageDataInPrefStoreForInstance(
      const base::UnguessableToken& instance_id,
      const base::TimeDelta& expected_usage_time) {
    const auto& usage_dict_pref =
        profile_->GetPrefs()->GetDict(::apps::kAppUsageTime);
    const auto& instance_id_string = instance_id.ToString();
    ASSERT_THAT(usage_dict_pref.Find(instance_id_string), NotNull());
    EXPECT_THAT(*usage_dict_pref.FindDict(instance_id_string)
                     ->FindString(::apps::kUsageTimeAppIdKey),
                StrEq(kTestAppId));
    EXPECT_THAT(base::ValueToTimeDelta(
                    usage_dict_pref.FindDict(instance_id_string)
                        ->Find(::apps::kReportingUsageTimeDurationKey)),
                Eq(expected_usage_time));
  }

  // Returns an `AppUsageData::AppUsage` proto message that tests can use to
  // test match with the actual one.
  const AppUsageData::AppUsage AppUsageProto(
      const base::UnguessableToken& instance_id,
      const base::TimeDelta& running_time) const {
    AppUsageData::AppUsage app_usage;
    app_usage.set_app_id(kTestAppPublisherId);
    app_usage.set_app_type(::apps::ApplicationType::APPLICATION_TYPE_UNKNOWN);
    app_usage.set_app_instance_id(instance_id.ToString());
    app_usage.set_running_time_ms(running_time.InMilliseconds());
    return app_usage;
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AppUsageTelemetrySampler> app_usage_telemetry_sampler_;

 private:
  raw_ptr<::ash::FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  std::unique_ptr<::user_manager::ScopedUserManager> scoped_user_manager_;
};

TEST_F(AppUsageTelemetrySamplerTest, CollectAppUsageDataForInstance) {
  // Simulate app usage so we have data in the pref store to work with.
  static constexpr base::TimeDelta kAppUsageDuration =
      base::Minutes(2) + base::Microseconds(200);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  CreateOrUpdateAppUsageForInstance(kInstanceId, kAppUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(::apps::kAppUsageTime).size(),
              Eq(1UL));

  // Attempt to collect this data and verify reported data.
  test::TestEvent<std::optional<MetricData>> test_event;
  app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
  const std::optional<MetricData> metric_data_result = test_event.result();
  ASSERT_TRUE(metric_data_result.has_value());
  const MetricData& metric_data = metric_data_result.value();
  ASSERT_TRUE(metric_data.has_telemetry_data());
  ASSERT_TRUE(metric_data.telemetry_data().has_app_telemetry());
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_usage_data());
  EXPECT_THAT(
      metric_data.telemetry_data().app_telemetry().app_usage_data().app_usage(),
      ElementsAre(EqualsProto(AppUsageProto(kInstanceId, kAppUsageDuration))));

  // Also verify usage data is reset in the pref store.
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, base::TimeDelta());
}

TEST_F(AppUsageTelemetrySamplerTest, NoAppUsageData) {
  test::TestEvent<std::optional<MetricData>> test_event;
  app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
  const std::optional<MetricData> metric_data_result = test_event.result();
  ASSERT_FALSE(metric_data_result.has_value());
}

TEST_F(AppUsageTelemetrySamplerTest, CollectResetAppUsageData) {
  // Simulate app usage so we have data in the pref store to work with.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  CreateOrUpdateAppUsageForInstance(kInstanceId, kAppUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(::apps::kAppUsageTime).size(),
              Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Attempt to collect this data and verify data is reset after it is reported.
  {
    test::TestEvent<std::optional<MetricData>> test_event;
    app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
    const std::optional<MetricData> metric_data_result = test_event.result();
    ASSERT_TRUE(metric_data_result.has_value());
    VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, base::TimeDelta());
  }

  // Attempt to collect data after it was reset in the previous step and verify
  // nothing is reported.
  {
    test::TestEvent<std::optional<MetricData>> test_event;
    app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
    const std::optional<MetricData> metric_data_result = test_event.result();
    ASSERT_FALSE(metric_data_result.has_value());
  }
}

TEST_F(AppUsageTelemetrySamplerTest, CollectSubsequentAppUsageData) {
  // Simulate app usage so we have data in the pref store to work with.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  CreateOrUpdateAppUsageForInstance(kInstanceId, kAppUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(::apps::kAppUsageTime).size(),
              Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Attempt to collect this data and verify data is reset after it is reported.
  {
    test::TestEvent<std::optional<MetricData>> test_event;
    app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
    const std::optional<MetricData> metric_data_result = test_event.result();
    ASSERT_TRUE(metric_data_result.has_value());
    VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, base::TimeDelta());
  }

  // Simulate additional usage after the previous collection.
  CreateOrUpdateAppUsageForInstance(kInstanceId, kAppUsageDuration);

  // Attempt to collect data and verify only data tracked from previous
  // collection is reported.
  {
    test::TestEvent<std::optional<MetricData>> test_event;
    app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
    const std::optional<MetricData> metric_data_result = test_event.result();
    ASSERT_TRUE(metric_data_result.has_value());
    const MetricData& metric_data = metric_data_result.value();
    ASSERT_TRUE(metric_data.has_telemetry_data());
    ASSERT_TRUE(metric_data.telemetry_data().has_app_telemetry());
    ASSERT_TRUE(
        metric_data.telemetry_data().app_telemetry().has_app_usage_data());
    EXPECT_THAT(metric_data.telemetry_data()
                    .app_telemetry()
                    .app_usage_data()
                    .app_usage(),
                ElementsAre(EqualsProto(
                    AppUsageProto(kInstanceId, kAppUsageDuration))));
    VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, base::TimeDelta());
  }
}

TEST_F(AppUsageTelemetrySamplerTest,
       CollectAppUsageDataAcrossMultipleInstances) {
  // Simulate app usage across instances so we have data in the pref store to
  // work with.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const base::UnguessableToken& kInstanceId1 = base::UnguessableToken::Create();
  const base::UnguessableToken& kInstanceId2 = base::UnguessableToken::Create();
  CreateOrUpdateAppUsageForInstance(kInstanceId1, kAppUsageDuration);
  CreateOrUpdateAppUsageForInstance(kInstanceId2, kAppUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(::apps::kAppUsageTime).size(),
              Eq(2UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId1, kAppUsageDuration);
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId2, kAppUsageDuration);

  // Attempt to collect usage data and verify data being reported.
  test::TestEvent<std::optional<MetricData>> test_event;
  app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
  const std::optional<MetricData> metric_data_result = test_event.result();
  ASSERT_TRUE(metric_data_result.has_value());
  const MetricData& metric_data = metric_data_result.value();
  ASSERT_TRUE(metric_data.has_telemetry_data());
  ASSERT_TRUE(metric_data.telemetry_data().has_app_telemetry());
  ASSERT_TRUE(
      metric_data.telemetry_data().app_telemetry().has_app_usage_data());
  EXPECT_THAT(
      metric_data.telemetry_data().app_telemetry().app_usage_data().app_usage(),
      UnorderedElementsAre(
          EqualsProto(AppUsageProto(kInstanceId1, kAppUsageDuration)),
          EqualsProto(AppUsageProto(kInstanceId2, kAppUsageDuration))));

  // Verify data is reset in the pref store now that it has been reported.
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId1, base::TimeDelta());
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId2, base::TimeDelta());
}

TEST_F(AppUsageTelemetrySamplerTest, CollectDataAfterProfileDestructed) {
  // Simulate app usage so we have data in the pref store to work with.
  static constexpr base::TimeDelta kAppUsageDuration = base::Minutes(2);
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  CreateOrUpdateAppUsageForInstance(kInstanceId, kAppUsageDuration);
  ASSERT_THAT(profile_->GetPrefs()->GetDict(::apps::kAppUsageTime).size(),
              Eq(1UL));
  VerifyAppUsageDataInPrefStoreForInstance(kInstanceId, kAppUsageDuration);

  // Destroy the test profile.
  profile_.reset();

  // Attempt to collect usage data and verify no data is being reported.
  test::TestEvent<std::optional<MetricData>> test_event;
  app_usage_telemetry_sampler_->MaybeCollect(test_event.cb());
  const std::optional<MetricData> metric_data_result = test_event.result();
  ASSERT_FALSE(metric_data_result.has_value());
}

}  // namespace
}  // namespace reporting
