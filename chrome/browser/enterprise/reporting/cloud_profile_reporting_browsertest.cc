// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service.h"
#include "chrome/browser/enterprise/reporting/cloud_profile_reporting_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/reporting/common_pref_names.h"
#include "components/enterprise/browser/reporting/reporting_features.h"
#include "components/policy/core/common/cloud/cloud_policy_service.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace enterprise_reporting {

namespace em = enterprise_management;

namespace {

constexpr char kDmToken1[] = "dm-token-1";
constexpr char kDeviceId1[] = "device-id-1";
constexpr char kDmToken2[] = "dm-token-2";
constexpr char kDeviceId2[] = "device-id-2";

void SetProfilePolicyData(Profile* profile,
                          const std::string& dm_token,
                          const std::string& device_id) {
  em::PolicyData policy_data;
  policy_data.set_request_token(dm_token);
  policy_data.set_device_id(device_id);
  profile->GetCloudPolicyManager()
      ->core()
      ->store()
      ->set_policy_data_for_testing(
          std::make_unique<em::PolicyData>(policy_data));
}

void EnableProfileManagement(Profile* profile,
                             const std::string& dm_token = "dm-token",
                             const std::string& device_id = "device-id") {
  SetProfilePolicyData(profile, dm_token, device_id);
  auto client = std::make_unique<policy::MockCloudPolicyClient>();
  client->SetDMToken(dm_token);
  client->SetClientId(device_id);
  profile->GetCloudPolicyManager()->core()->ConnectForTesting(
      /*service=*/nullptr, std::move(client));
}

void SetReportingPolicy(Profile* profile, bool enabled) {
  profile->GetPrefs()->SetBoolean(kCloudProfileReportingEnabled, enabled);
}

void DisableProfileManagement(Profile* profile) {
  profile->GetCloudPolicyManager()->core()->Disconnect();
  profile->GetCloudPolicyManager()
      ->core()
      ->store()
      ->set_policy_data_for_testing(nullptr);
  SetReportingPolicy(profile, false);
  profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, false);
}

}  // namespace

class CloudProfileReportingServiceTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<
          // Two boolean variables represents whether profile reporting and
          // signals reporting is enabled
          std::tuple<bool, bool>> {
 public:
  CloudProfileReportingServiceTest() {
    scoped_feature_list_.InitAndDisableFeature(kUploadReportOnProfileOpen);
  }
  ~CloudProfileReportingServiceTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = chrome_test_utils::GetProfile(this);
    ASSERT_TRUE(CloudProfileReportingServiceFactory::GetForProfile(profile));
    EnableProfileManagement(profile);
    SetReportingPolicy(profile, profile_reporting_enabled());
    profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting,
                                    signals_reporting_enabled());
  }

  bool profile_reporting_enabled() const { return std::get<0>(GetParam()); }
  bool signals_reporting_enabled() const { return std::get<1>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CloudProfileReportingServiceTest,
                       VerifyReportingConfig) {
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);

  bool expect_report =
      profile_reporting_enabled() || signals_reporting_enabled();
  if (expect_report) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return report_scheduler->GetActiveTriggerForTesting() !=
             ReportTrigger::kTriggerNone;
    }));
  }

  auto active_trigger = report_scheduler->GetActiveTriggerForTesting();
  auto active_config = report_scheduler->GetActiveGenerationConfigForTesting();

  if (signals_reporting_enabled() && profile_reporting_enabled()) {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerTimer);
    EXPECT_EQ(active_config.security_signals_mode,
              SecuritySignalsMode::kSignalsAttached);
  } else if (profile_reporting_enabled()) {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerTimer);
    EXPECT_EQ(active_config.security_signals_mode,
              SecuritySignalsMode::kNoSignals);
  } else if (signals_reporting_enabled()) {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerSecurity);
    EXPECT_EQ(active_config.security_signals_mode,
              SecuritySignalsMode::kSignalsOnly);
  } else {
    EXPECT_EQ(active_trigger, ReportTrigger::kTriggerNone);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CloudProfileReportingServiceTest,
                         testing::Combine(
                             /*profile_reporting_enabled=*/testing::Bool(),
                             /*signals_reporting_enabled=*/testing::Bool()));

class UploadReportOnProfileOpenBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool, bool>> {
 public:
  UploadReportOnProfileOpenBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(
        kUploadReportOnProfileOpen, upload_report_on_profile_open_enabled());
  }
  ~UploadReportOnProfileOpenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    Profile* profile = chrome_test_utils::GetProfile(this);
    ASSERT_TRUE(CloudProfileReportingServiceFactory::GetForProfile(profile));
    SetReportingPolicy(profile, profile_reporting_enabled());
    profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting,
                                    signals_reporting_enabled());
    // Set a recent upload timestamp so that the 24h periodic timer does not
    // expire on startup.
    profile->GetPrefs()->SetTime(kLastUploadTimestamp, base::Time::Now());
    EnableProfileManagement(profile);
  }

  bool upload_report_on_profile_open_enabled() const {
    return std::get<0>(GetParam());
  }
  bool profile_reporting_enabled() const { return std::get<1>(GetParam()); }
  bool signals_reporting_enabled() const { return std::get<2>(GetParam()); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(UploadReportOnProfileOpenBrowserTest,
                       VerifyProfileOpenTrigger) {
  ReportScheduler* report_scheduler =
      CloudProfileReportingServiceFactory::GetForProfile(
          chrome_test_utils::GetProfile(this))
          ->report_scheduler();
  ASSERT_TRUE(report_scheduler);

  bool expect_report =
      upload_report_on_profile_open_enabled()
          ? (profile_reporting_enabled() || signals_reporting_enabled())
          : signals_reporting_enabled();

  if (expect_report) {
    EXPECT_TRUE(base::test::RunUntil([&]() {
      return report_scheduler->GetActiveTriggerForTesting() !=
             ReportTrigger::kTriggerNone;
    }));
  }

  auto active_trigger = report_scheduler->GetActiveTriggerForTesting();
  auto active_config = report_scheduler->GetActiveGenerationConfigForTesting();

  if (upload_report_on_profile_open_enabled()) {
    // When kUploadReportOnProfileOpen is enabled, a report is uploaded upon
    // profile open even if a report was uploaded recently.
    if (profile_reporting_enabled() && signals_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerProfileOpened);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kSignalsAttached);
    } else if (profile_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerProfileOpened);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kNoSignals);
    } else if (signals_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerProfileOpened);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kSignalsOnly);
    } else {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerNone);
    }
  } else {
    // When kUploadReportOnProfileOpen is disabled:
    // Periodic profile reporting is not triggered since the last upload is
    // recent, but legacy security signal reporting triggers its own report on
    // startup if enabled.
    if (signals_reporting_enabled()) {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerSecurity);
      EXPECT_EQ(active_config.security_signals_mode,
                SecuritySignalsMode::kSignalsOnly);
    } else {
      EXPECT_EQ(active_trigger, ReportTrigger::kTriggerNone);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UploadReportOnProfileOpenBrowserTest,
    testing::Combine(
        /*upload_report_on_profile_open_enabled=*/testing::Bool(),
        /*profile_reporting_enabled=*/testing::Bool(),
        /*signals_reporting_enabled=*/testing::Bool()));

class CloudProfileReportingAccountSwitchBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  CloudProfileReportingAccountSwitchBrowserTest() {
    scoped_feature_list_.InitWithFeatureState(kUploadReportOnProfileOpen,
                                              GetParam());
  }
  ~CloudProfileReportingAccountSwitchBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(CloudProfileReportingAccountSwitchBrowserTest,
                       SwitchAccountResetsSecuritySignalsDMToken) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  CloudProfileReportingService* service =
      CloudProfileReportingServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());

  // Disconnecting when no scheduler has been created is a safe no-op.
  DisableProfileManagement(profile);
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());

  // 1. Enable security signals reporting before connecting User 1 so that
  // ReportScheduler's constructor exercises kUploadReportOnProfileOpen
  // (GetParam()).
  profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  profile->GetPrefs()->SetTime(kLastUploadTimestamp, base::Time::Now());
  EnableProfileManagement(profile, kDmToken1, kDeviceId1);

  ASSERT_TRUE(service->report_scheduler());
  ASSERT_TRUE(service->cloud_policy_client());
  const ReportTrigger expected_initial_trigger =
      GetParam() ? ReportTrigger::kTriggerProfileOpened
                 : ReportTrigger::kTriggerSecurity;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service->report_scheduler()->GetActiveTriggerForTesting() ==
           expected_initial_trigger;
  }));
  EXPECT_EQ(service->report_scheduler()
                ->GetActiveGenerationConfigForTesting()
                .security_signals_mode,
            SecuritySignalsMode::kSignalsOnly);
  EXPECT_EQ(service->cloud_policy_client()->dm_token(), kDmToken1);
  EXPECT_EQ(service->cloud_policy_client()->client_id(), kDeviceId1);

  // 2. Sign out User 1 on the same Profile (mirrors Android sign-out).
  DisableProfileManagement(profile);
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());
  EXPECT_FALSE(service->saas_usage_report_scheduler());

  // 3. Sign in as User 2 with both profile and security signals reporting
  // enabled and verify uploads use User 2's DMToken and client ID rather than
  // User 1's stale registration.
  SetReportingPolicy(profile, true);
  profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  profile->GetPrefs()->SetTime(kLastUploadTimestamp, base::Time::Now());
  EnableProfileManagement(profile, kDmToken2, kDeviceId2);

  ASSERT_TRUE(service->report_scheduler());
  ASSERT_TRUE(service->cloud_policy_client());
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service->report_scheduler()->GetActiveTriggerForTesting() ==
           expected_initial_trigger;
  }));
  EXPECT_EQ(service->cloud_policy_client()->dm_token(), kDmToken2);
  EXPECT_EQ(service->cloud_policy_client()->client_id(), kDeviceId2);

  // 4. Sign out User 2 to confirm core_observation_ remains active across
  // multiple connect/disconnect cycles.
  DisableProfileManagement(profile);
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());
  EXPECT_FALSE(service->saas_usage_report_scheduler());
}

IN_PROC_BROWSER_TEST_P(CloudProfileReportingAccountSwitchBrowserTest,
                       OfflineSignOutThenOnlineSignInResetsDMToken) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  CloudProfileReportingService* service =
      CloudProfileReportingServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  // 1. Simulate offline launch with cached User 1 policy (core is NOT connected
  // yet, so core->client() is null).
  SetProfilePolicyData(profile, kDmToken1, kDeviceId1);
  profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  profile->GetPrefs()->SetTime(kLastUploadTimestamp, base::Time::Now());
  service->CreateReportScheduler();

  ASSERT_TRUE(service->report_scheduler());
  ASSERT_TRUE(service->cloud_policy_client());
  const ReportTrigger expected_trigger =
      GetParam() ? ReportTrigger::kTriggerProfileOpened
                 : ReportTrigger::kTriggerSecurity;
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service->report_scheduler()->GetActiveTriggerForTesting() ==
           expected_trigger;
  }));
  EXPECT_EQ(service->cloud_policy_client()->dm_token(), kDmToken1);
  EXPECT_EQ(service->cloud_policy_client()->client_id(), kDeviceId1);

  // 2. Simulate offline sign-out: because core->client() is null,
  // Disconnect() does NOT fire OnCoreDisconnecting().
  ASSERT_FALSE(profile->GetCloudPolicyManager()->core()->client());
  DisableProfileManagement(profile);
  ASSERT_TRUE(service->cloud_policy_client());
  ASSERT_TRUE(service->cloud_policy_client()->is_registered());

  // 3. Sign in online as User 2: because User 2's DMToken differs from
  // active_dm_token_, OnCoreConnected() synchronously recreates an
  // unregistered client and then registers with User 2's DMToken.
  EnableProfileManagement(profile, kDmToken2, kDeviceId2);
  ASSERT_TRUE(service->report_scheduler());
  ASSERT_TRUE(service->cloud_policy_client());
  EXPECT_FALSE(service->cloud_policy_client()->is_registered());

  profile->GetPrefs()->SetBoolean(kUserSecuritySignalsReporting, true);
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return service->report_scheduler()->GetActiveTriggerForTesting() !=
           ReportTrigger::kTriggerNone;
  }));
  EXPECT_EQ(service->cloud_policy_client()->dm_token(), kDmToken2);
  EXPECT_EQ(service->cloud_policy_client()->client_id(), kDeviceId2);
}

IN_PROC_BROWSER_TEST_P(CloudProfileReportingAccountSwitchBrowserTest,
                       SameUserConnectionDoesNotRecreateScheduler) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  CloudProfileReportingService* service =
      CloudProfileReportingServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  // Simulate startup where Init()'s task already created the scheduler from
  // cached policy before CloudPolicyCore finished connecting online.
  SetProfilePolicyData(profile, kDmToken1, kDeviceId1);
  service->CreateReportScheduler();
  ReportScheduler* initial_scheduler = service->report_scheduler();
  policy::CloudPolicyClient* initial_client = service->cloud_policy_client();
  ASSERT_TRUE(initial_scheduler);
  ASSERT_TRUE(initial_client);

  // Connecting CloudPolicyCore for the same DMToken keeps the existing
  // scheduler and client intact rather than tearing down in-flight work.
  EnableProfileManagement(profile, kDmToken1, kDeviceId1);
  EXPECT_EQ(service->report_scheduler(), initial_scheduler);
  EXPECT_EQ(service->cloud_policy_client(), initial_client);
}

IN_PROC_BROWSER_TEST_P(CloudProfileReportingAccountSwitchBrowserTest,
                       DisconnectCancelsPendingInitTask) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  CloudProfileReportingService* service =
      CloudProfileReportingServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  EnableProfileManagement(profile, kDmToken1, kDeviceId1);
  ASSERT_TRUE(service->report_scheduler());

  // Queue Init()'s BEST_EFFORT CreateReportScheduler() task, then disconnect
  // before it executes.
  service->InitForTesting();
  DisableProfileManagement(profile);
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());

  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());
}

IN_PROC_BROWSER_TEST_P(CloudProfileReportingAccountSwitchBrowserTest,
                       CoreDestructionResetsSchedulersAndStopsObserving) {
  Profile* profile = chrome_test_utils::GetProfile(this);
  CloudProfileReportingService* service =
      CloudProfileReportingServiceFactory::GetForProfile(profile);
  ASSERT_TRUE(service);

  SetProfilePolicyData(profile, kDmToken1, kDeviceId1);
  service->CreateReportScheduler();
  ASSERT_TRUE(service->report_scheduler());
  ASSERT_TRUE(service->cloud_policy_client());

  service->OnCoreDestruction(profile->GetCloudPolicyManager()->core());
  EXPECT_FALSE(service->report_scheduler());
  EXPECT_FALSE(service->cloud_policy_client());
  EXPECT_FALSE(service->saas_usage_report_scheduler());
}

INSTANTIATE_TEST_SUITE_P(All,
                         CloudProfileReportingAccountSwitchBrowserTest,
                         /*upload_report_on_profile_open_enabled=*/
                         testing::Bool());

}  // namespace enterprise_reporting
