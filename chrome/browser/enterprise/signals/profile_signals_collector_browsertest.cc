// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/profile_signals_collector.h"

#include <array>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#include "components/enterprise/isolated_mode/prefs.h"
#include "components/policy/core/common/cloud/cloud_policy_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeUserEnrollmentDomain[] = "fake.domain.google.com";
constexpr char kFakeProfileId[] = "SomeProfileId";

constexpr char kCollectionSuccessHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success";
constexpr char kCollectionFailureHistogram[] =
    "Enterprise.DeviceSignals.Collection.Failure";
constexpr char kBrowserContextSignalsSuccessLatencyHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success.BrowserContextSignals."
    "Latency";

std::unique_ptr<KeyedService> CreateProfileIdService(
    content::BrowserContext* context) {
  return std::make_unique<enterprise::ProfileIdService>(kFakeProfileId);
}

}  // namespace

namespace device_signals {

class ProfileSignalsCollectorTest : public PlatformBrowserTest {
 protected:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    enterprise::ProfileIdServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateProfileIdService));
    PlatformBrowserTest::SetUpBrowserContextKeyedServices(context);
  }

  std::unique_ptr<ProfileSignalsCollector> CreateProfileSignalsCollector() {
    return std::make_unique<ProfileSignalsCollector>(profile());
  }

  void SetFakePolicyAndPrefData() {
    auto policy_data = std::make_unique<enterprise_management::PolicyData>();
    policy_data->set_managed_by(kFakeUserEnrollmentDomain);
    profile()
        ->GetCloudPolicyManager()
        ->core()
        ->store()
        ->set_policy_data_for_testing(std::move(policy_data));

    g_browser_process->local_state()->SetBoolean(
        prefs::kBuiltInDnsClientEnabled, true);

    // Give the testing profile a safe browsing level of "STANDARD_PROTECTION"
    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
    profile()->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
  }

  // Helper function to check the profile level signals are collected correctly.
  void CheckSignalsCollected(ProfileSignalsResponse& response,
                             bool expected_is_isolated_mode = false) {
    EXPECT_EQ(response.profile_enrollment_domain, kFakeUserEnrollmentDomain);
    EXPECT_EQ(response.safe_browsing_protection_level,
              safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
    EXPECT_TRUE(response.built_in_dns_client_enabled);
    EXPECT_EQ(response.is_isolated_mode, expected_is_isolated_mode);
  }

  // Verifies that exactly one successful browser context signals collection
  // was recorded.
  void ExpectCollectionSuccessLogged() {
    histogram_tester_.ExpectUniqueSample(kCollectionSuccessHistogram,
                                         SignalName::kBrowserContextSignals, 1);
    histogram_tester_.ExpectTotalCount(
        kBrowserContextSignalsSuccessLatencyHistogram, 1);
    histogram_tester_.ExpectTotalCount(kCollectionFailureHistogram, 0);
  }

  // Verifies that no browser context signals collection outcome was recorded.
  void ExpectNoCollectionOutcomeLogged() {
    histogram_tester_.ExpectTotalCount(kCollectionSuccessHistogram, 0);
    histogram_tester_.ExpectTotalCount(
        kBrowserContextSignalsSuccessLatencyHistogram, 0);
    histogram_tester_.ExpectTotalCount(kCollectionFailureHistogram, 0);
  }

  Profile* profile() { return chrome_test_utils::GetProfile(this); }
  base::HistogramTester histogram_tester_;
  std::unique_ptr<ProfileSignalsCollector> signal_collector_ = nullptr;
};

// Test that runs a sanity check on the set of signals supported by this
// collector. Will need to be updated if new signals become supported.
IN_PROC_BROWSER_TEST_F(ProfileSignalsCollectorTest,
                       SupportedBrowserContextSignalNames) {
  auto signals_collector = CreateProfileSignalsCollector();
  const std::array<SignalName, 1> supported_signals{
      {SignalName::kBrowserContextSignals}};

  const auto names_set = signals_collector->GetSupportedSignalNames();

  EXPECT_EQ(names_set.size(), supported_signals.size());
  for (const auto& signal_name : supported_signals) {
    EXPECT_TRUE(names_set.find(signal_name) != names_set.end());
  }
}

// Happy path test case for OS signals collection with full permission.
IN_PROC_BROWSER_TEST_F(ProfileSignalsCollectorTest, GetSignal_Success) {
  auto signals_collector = CreateProfileSignalsCollector();
  SetFakePolicyAndPrefData();

  SignalName signal_name = SignalName::kBrowserContextSignals;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  signals_collector->GetSignal(signal_name, UserPermission::kGranted,
                               empty_request, response, base::DoNothing());

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.profile_signals_response);
  CheckSignalsCollected(response.profile_signals_response.value(),
                        /*expected_is_isolated_mode=*/false);
  ExpectCollectionSuccessLogged();
}

// Tests that an unsupported signal is marked as unsupported.
IN_PROC_BROWSER_TEST_F(ProfileSignalsCollectorTest,
                       GetBrowserContextSignal_Unsupported) {
  auto signals_collector = CreateProfileSignalsCollector();
  SignalName signal_name = SignalName::kAntiVirus;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  signals_collector->GetSignal(signal_name, UserPermission::kGranted,
                               empty_request, response, base::DoNothing());

  ASSERT_TRUE(response.top_level_error.has_value());
  EXPECT_EQ(response.top_level_error.value(),
            SignalCollectionError::kUnsupported);
  ExpectNoCollectionOutcomeLogged();
}

// Tests that signal collection is halted if permission is not sufficient.
IN_PROC_BROWSER_TEST_F(ProfileSignalsCollectorTest, GetSignal_MissingUser) {
  auto signals_collector = CreateProfileSignalsCollector();
  SignalName signal_name = SignalName::kBrowserContextSignals;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  signals_collector->GetSignal(signal_name, UserPermission::kMissingUser,
                               empty_request, response, base::DoNothing());

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_FALSE(response.profile_signals_response);
  ExpectNoCollectionOutcomeLogged();
}

class ProfileSignalsCollectorIsolatedModeTest
    : public ProfileSignalsCollectorTest {
 protected:
  ProfileSignalsCollectorIsolatedModeTest() {
    feature_list_.InitWithFeatures(
        {enterprise_isolated_mode::kEnableEnterpriseIsolatedMode,
         enterprise_isolated_mode::kEnterpriseIsolatedModeMilestone2},
        {});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    ProfileSignalsCollectorTest::SetUpBrowserContextKeyedServices(context);
    SetupIsolatedMode(Profile::FromBrowserContext(context));
  }

 private:
  void SetupIsolatedMode(Profile* profile) {
    ASSERT_TRUE(profile);
    profile->GetPrefs()->SetInteger(
        enterprise_isolated_mode::kEnterpriseIsolatedModeSettings,
        static_cast<int>(
            enterprise_isolated_mode::IsolatedModeSetting::kEnabled));
  }
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ProfileSignalsCollectorIsolatedModeTest,
                       GetSignal_IsolatedModeProfile) {
  SetFakePolicyAndPrefData();
  Profile* isolated_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  ASSERT_TRUE(isolated_profile->IsEnterpriseIsolatedModeProfile());

  ProfileSignalsCollector signals_collector(isolated_profile);
  SignalName signal_name = SignalName::kBrowserContextSignals;
  SignalsAggregationRequest empty_request;
  SignalsAggregationResponse response;
  signals_collector.GetSignal(signal_name, UserPermission::kGranted,
                              empty_request, response, base::DoNothing());

  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.profile_signals_response);
  CheckSignalsCollected(response.profile_signals_response.value(),
                        /*expected_is_isolated_mode=*/true);
  ExpectCollectionSuccessLogged();
}

}  // namespace device_signals
