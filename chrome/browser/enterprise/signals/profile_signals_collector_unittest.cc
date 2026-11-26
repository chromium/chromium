// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/profile_signals_collector.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/device_signals/core/browser/signals_types.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <optional>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/enterprise/signals/app_bound_encryption_test_utils_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_provider_win.h"
#include "chrome/browser/os_crypt/app_bound_encryption_win.h"
#include "components/device_signals/core/common/signals_features.h"
#include "components/os_crypt/async/browser/key_provider.h"
#endif  // BUILDFLAG(IS_WIN)

namespace device_signals {

namespace {

constexpr char kFakeProfileId[] = "FakeProfileId";

constexpr char kCollectionSuccessHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success";
constexpr char kCollectionFailureHistogram[] =
    "Enterprise.DeviceSignals.Collection.Failure";
constexpr char kBrowserContextSignalsSuccessLatencyHistogram[] =
    "Enterprise.DeviceSignals.Collection.Success.BrowserContextSignals.Latency";

std::unique_ptr<KeyedService> CreateProfileIdService(
    content::BrowserContext* context) {
  return std::make_unique<enterprise::ProfileIdService>(kFakeProfileId);
}

}  // namespace

class ProfileSignalsCollectorUnitTest : public testing::Test {
 protected:
#if BUILDFLAG(IS_WIN)
  explicit ProfileSignalsCollectorUnitTest(
      bool enable_device_trust_signal = false,
      bool enable_reporting_signal = false) {
    scoped_feature_list_.InitWithFeatureStates(
        {{enterprise_signals::features::
              kDeviceTrustAppBoundEncryptionStateSignal,
          enable_device_trust_signal},
         {enterprise_signals::features::kReportAppBoundEncryptionState,
          enable_reporting_signal}});
  }
#endif  // BUILDFLAG(IS_WIN)

  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        enterprise::ProfileIdServiceFactory::GetInstance(),
        base::BindRepeating(&CreateProfileIdService));
    profile_ = builder.Build();
    collector_ = std::make_unique<ProfileSignalsCollector>(profile_.get());
  }

  SignalsAggregationResponse CollectSignals(
      Trigger trigger = Trigger::kBrowserNavigation) {
    SignalsAggregationRequest request;
    request.trigger = trigger;
    SignalsAggregationResponse response;
    base::test::TestFuture<void> done_future;
    collector_->GetSignal(SignalName::kBrowserContextSignals,
                          UserPermission::kGranted, request, response,
                          done_future.GetCallback());
    EXPECT_TRUE(done_future.Wait());
    return response;
  }

#if BUILDFLAG(IS_WIN)
  base::test::ScopedFeatureList scoped_feature_list_;
  test::ScopedAppBoundEncryptionOverrides app_bound_overrides_;
#endif  // BUILDFLAG(IS_WIN)
  content::BrowserTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<ProfileSignalsCollector> collector_;
};

TEST_F(ProfileSignalsCollectorUnitTest, CollectSignals_BasicFields) {
  TestingBrowserProcess::GetGlobal()->local_state()->SetBoolean(
      prefs::kBuiltInDnsClientEnabled, true);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnabled, true);
  profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);

  SignalsAggregationResponse response = CollectSignals();
  ASSERT_FALSE(response.top_level_error.has_value());
  ASSERT_TRUE(response.profile_signals_response.has_value());

  const ProfileSignalsResponse& profile_signals =
      response.profile_signals_response.value();
  EXPECT_TRUE(profile_signals.built_in_dns_client_enabled);
  EXPECT_EQ(profile_signals.safe_browsing_protection_level,
            safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  EXPECT_EQ(profile_signals.profile_id, kFakeProfileId);
  EXPECT_FALSE(profile_signals.app_bound_encryption_state.has_value());

  histogram_tester_.ExpectUniqueSample(kCollectionSuccessHistogram,
                                       SignalName::kBrowserContextSignals, 1);
  histogram_tester_.ExpectTotalCount(
      kBrowserContextSignalsSuccessLatencyHistogram, 1);
  histogram_tester_.ExpectTotalCount(kCollectionFailureHistogram, 0);
}

#if BUILDFLAG(IS_WIN)
class ProfileSignalsCollectorAppBoundDisabledTest
    : public ProfileSignalsCollectorUnitTest {
 protected:
  ProfileSignalsCollectorAppBoundDisabledTest()
      : ProfileSignalsCollectorUnitTest(/*enable_device_trust_signal=*/false) {}
};

TEST_F(ProfileSignalsCollectorAppBoundDisabledTest,
       AppBoundEncryptionState_FeatureDisabled) {
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::FakeAppBoundKeyProvider>(
              std::string(os_crypt_async::kAppBoundDataPrefix)));

  SignalsAggregationResponse response = CollectSignals();
  ASSERT_TRUE(response.profile_signals_response.has_value());
  EXPECT_FALSE(response.profile_signals_response->app_bound_encryption_state
                   .has_value());
}

class ProfileSignalsCollectorAppBoundEnabledTest
    : public ProfileSignalsCollectorUnitTest {
 protected:
  ProfileSignalsCollectorAppBoundEnabledTest()
      : ProfileSignalsCollectorUnitTest(/*enable_device_trust_signal=*/true) {}
};

TEST_F(ProfileSignalsCollectorAppBoundEnabledTest,
       AppBoundEncryptionState_NonNavigationTriggerIgnored) {
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::FakeAppBoundKeyProvider>(
              std::string(os_crypt_async::kAppBoundDataPrefix)));

  for (Trigger trigger : {Trigger::kUnspecified, Trigger::kSignalsReport}) {
    SCOPED_TRACE(static_cast<int>(trigger));
    SignalsAggregationResponse response = CollectSignals(trigger);
    ASSERT_TRUE(response.profile_signals_response.has_value());
    EXPECT_FALSE(response.profile_signals_response->app_bound_encryption_state
                     .has_value());
  }
}

TEST_F(ProfileSignalsCollectorAppBoundEnabledTest,
       AppBoundEncryptionState_Enabled) {
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::FakeAppBoundKeyProvider>(
              std::string(os_crypt_async::kAppBoundDataPrefix)));

  SignalsAggregationResponse response = CollectSignals();
  ASSERT_TRUE(response.profile_signals_response.has_value());
  EXPECT_EQ(response.profile_signals_response->app_bound_encryption_state,
            AppBoundEncryptionState::kEnabled);

  histogram_tester_.ExpectUniqueSample(kCollectionSuccessHistogram,
                                       SignalName::kBrowserContextSignals, 1);
  histogram_tester_.ExpectTotalCount(
      kBrowserContextSignalsSuccessLatencyHistogram, 1);
}

TEST_F(ProfileSignalsCollectorAppBoundEnabledTest,
       AppBoundEncryptionState_DisabledByPolicy) {
  app_bound_overrides_.set_support_level(
      os_crypt::SupportLevel::kDisabledByPolicy);
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::FakeAppBoundKeyProvider>("v10"));

  SignalsAggregationResponse response = CollectSignals();
  ASSERT_TRUE(response.profile_signals_response.has_value());
  EXPECT_EQ(response.profile_signals_response->app_bound_encryption_state,
            AppBoundEncryptionState::kDisabledByPolicy);
}

TEST_F(ProfileSignalsCollectorAppBoundEnabledTest,
       AppBoundEncryptionState_DeferredOSCryptAsyncCallback) {
  base::test::TestFuture<os_crypt_async::KeyProvider::KeyCallback>
      key_callback_future;
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::DeferredFakeAppBoundKeyProvider>(
              /*use_for_encryption=*/true, key_callback_future.GetCallback()));

  SignalsAggregationRequest request;
  request.trigger = Trigger::kBrowserNavigation;
  SignalsAggregationResponse response;
  base::test::TestFuture<void> done_future;
  collector_->GetSignal(SignalName::kBrowserContextSignals,
                        UserPermission::kGranted, request, response,
                        done_future.GetCallback());
  EXPECT_FALSE(done_future.IsReady());
  ASSERT_TRUE(key_callback_future.Wait());

  key_callback_future.Take().Run(
      std::string(os_crypt_async::kAppBoundDataPrefix),
      test::CreateFakeAppBoundKey());

  EXPECT_TRUE(done_future.Wait());
  ASSERT_TRUE(response.profile_signals_response.has_value());
  EXPECT_EQ(response.profile_signals_response->app_bound_encryption_state,
            AppBoundEncryptionState::kEnabled);

  histogram_tester_.ExpectUniqueSample(kCollectionSuccessHistogram,
                                       SignalName::kBrowserContextSignals, 1);
  histogram_tester_.ExpectTotalCount(
      kBrowserContextSignalsSuccessLatencyHistogram, 1);
}

TEST_F(ProfileSignalsCollectorAppBoundEnabledTest,
       AppBoundEncryptionState_CollectorDestroyedBeforeCallbackCompletes) {
  base::test::TestFuture<os_crypt_async::KeyProvider::KeyCallback>
      key_callback_future;
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::DeferredFakeAppBoundKeyProvider>(
              /*use_for_encryption=*/true, key_callback_future.GetCallback()));

  SignalsAggregationRequest request;
  request.trigger = Trigger::kBrowserNavigation;
  std::optional<SignalsAggregationResponse> response{std::in_place};
  base::test::TestFuture<void> done_future;
  collector_->GetSignal(SignalName::kBrowserContextSignals,
                        UserPermission::kGranted, request, *response,
                        done_future.GetCallback());
  ASSERT_TRUE(key_callback_future.Wait());

  collector_.reset();
  response.reset();

  key_callback_future.Take().Run(
      std::string(os_crypt_async::kAppBoundDataPrefix),
      test::CreateFakeAppBoundKey());

  EXPECT_FALSE(done_future.IsReady());
  histogram_tester_.ExpectTotalCount(kCollectionSuccessHistogram, 0);
}

TEST_F(ProfileSignalsCollectorAppBoundDisabledTest,
       AppBoundEncryptionState_SignalsReportDisabled) {
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::FakeAppBoundKeyProvider>(
              std::string(os_crypt_async::kAppBoundDataPrefix)));

  SignalsAggregationResponse response = CollectSignals(Trigger::kSignalsReport);
  ASSERT_TRUE(response.profile_signals_response.has_value());
  EXPECT_FALSE(response.profile_signals_response->app_bound_encryption_state
                   .has_value());
}

class ProfileSignalsCollectorAppBoundReportingEnabledTest
    : public ProfileSignalsCollectorUnitTest {
 protected:
  ProfileSignalsCollectorAppBoundReportingEnabledTest()
      : ProfileSignalsCollectorUnitTest(/*enable_device_trust_signal=*/false,
                                        /*enable_reporting_signal=*/true) {}
};

TEST_F(ProfileSignalsCollectorAppBoundReportingEnabledTest,
       AppBoundEncryptionState_SignalsReportEnabledIndependentlyOfDtc) {
  TestingBrowserProcess::GetGlobal()
      ->set_additional_os_crypt_async_provider_for_test(
          /*precedence=*/10u,
          std::make_unique<test::FakeAppBoundKeyProvider>(
              std::string(os_crypt_async::kAppBoundDataPrefix)));

  SignalsAggregationResponse report_response =
      CollectSignals(Trigger::kSignalsReport);
  ASSERT_TRUE(report_response.profile_signals_response.has_value());
  EXPECT_EQ(
      report_response.profile_signals_response->app_bound_encryption_state,
      AppBoundEncryptionState::kEnabled);

  for (Trigger trigger : {Trigger::kUnspecified, Trigger::kBrowserNavigation}) {
    SCOPED_TRACE(static_cast<int>(trigger));
    SignalsAggregationResponse other_response = CollectSignals(trigger);
    ASSERT_TRUE(other_response.profile_signals_response.has_value());
    EXPECT_FALSE(other_response.profile_signals_response
                     ->app_bound_encryption_state.has_value());
  }
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace device_signals
