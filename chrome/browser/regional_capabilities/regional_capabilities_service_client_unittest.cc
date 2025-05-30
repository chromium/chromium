// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/regional_capabilities/android/test_utils_jni_headers/RegionalCapabilitiesServiceTestUtil_jni.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#endif

using ::country_codes::CountryId;

namespace regional_capabilities {

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr char kBelgiumCountryCode[] = "BE";

constexpr CountryId kBelgiumCountryId(kBelgiumCountryCode);

class TestSupportAndroid {
 public:
  TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_ref =
        Java_RegionalCapabilitiesServiceTestUtil_Constructor(env);
    java_test_util_ref_.Reset(env, java_ref.obj());
  }

  ~TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_RegionalCapabilitiesServiceTestUtil_destroy(env, java_test_util_ref_);
  }

  void ReturnDeviceCountry(const std::string& device_country) {
    JNIEnv* env = base::android::AttachCurrentThread();
    Java_RegionalCapabilitiesServiceTestUtil_returnDeviceCountry(
        env, java_test_util_ref_,
        base::android::ConvertUTF8ToJavaString(env, device_country));
  }

  void TriggerDeviceCountryFailure() {
    JNIEnv* env = base::android::AttachCurrentThread();

    Java_RegionalCapabilitiesServiceTestUtil_triggerDeviceCountryFailure(
        env, java_test_util_ref_);
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_test_util_ref_;
};
#endif

}  // namespace

class RegionalCapabilitiesServiceClientTest : public ::testing::Test {
#if BUILDFLAG(IS_CHROMEOS)
 public:
  void SetUp() override {
    ash::system::StatisticsProvider::SetTestProvider(&sys_info_);
  }

  void TearDown() override {
    ash::system::StatisticsProvider::SetTestProvider(nullptr);
  }

  void SetLoadingState(ash::system::StatisticsProvider::LoadingState state) {
    sys_info_.SetLoadingState(state);
  }

  void SetRegion(const std::string& region) {
    sys_info_.SetMachineStatistic(ash::system::kRegionKey, region);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::HistogramTester histogram_tester_;
  ash::system::FakeStatisticsProvider sys_info_;
#endif
};

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_LoadingState) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 0);

  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kNotStarted);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 1);

  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kStarted);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 2);

  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData,
      ChromeOSFallbackCountry::kStatisticsLoadingNotFinished, 2);
}

TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_GroupedRegions) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kGroupedRegion, 0);
  int i = 0;
  for (const std::string region : {"gcc", "LaTaM-Es-419", "NORDIC"}) {
    SetRegion(region);
    histogram_tester().ExpectUniqueSample(
        kCrOSMissingVariationData, ChromeOSFallbackCountry::kGroupedRegion, i);
    EXPECT_EQ(client.GetFallbackCountryId(),
              country_codes::GetCurrentCountryID());
    histogram_tester().ExpectUniqueSample(
        kCrOSMissingVariationData, ChromeOSFallbackCountry::kGroupedRegion,
        ++i);
  }
}

TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_RegionTooShort) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectTotalCount(kCrOSMissingVariationData, 0);
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionAbsent, 1);

  SetRegion("");
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionAbsent, 1);
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionEmpty, 1);

  SetRegion("a");
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionAbsent, 1);
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionEmpty, 1);
  histogram_tester().ExpectBucketCount(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionTooShort, 1);
}

TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_RegionTooLong) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionTooLong, 0);
  SetRegion("en_US");
  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kRegionTooLong, 1);
}

TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_ValidRegion) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 0);
  std::string country_code = "DE";
  if (country_code == country_codes::GetCurrentCountryID().CountryCode()) {
    country_code = "BE";
  }
  SetRegion(country_code);
  const CountryId fallback_id = client.GetFallbackCountryId();
  ASSERT_NE(fallback_id, country_codes::GetCurrentCountryID());
  EXPECT_EQ(fallback_id, country_codes::CountryId(country_code));
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 1);
}

TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_StripKeyboardLayout) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 0);
  std::string country_code = "DE";
  if (country_code == country_codes::GetCurrentCountryID().CountryCode()) {
    country_code = "BE";
  }
  const std::string country_with_layout_info = country_code + ".us-intl";
  SetRegion(country_with_layout_info);
  const CountryId fallback_id = client.GetFallbackCountryId();
  ASSERT_NE(fallback_id, country_codes::GetCurrentCountryID());
  EXPECT_EQ(fallback_id, country_codes::CountryId(country_code));
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 1);
  histogram_tester().ExpectUniqueSample(kVpdRegionSplittingOutcome, true, 1);
}

TEST_F(RegionalCapabilitiesServiceClientTest,
       GetFallbackCountryId_StripKeyboardLayoutFailure) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
  SetLoadingState(ash::system::StatisticsProvider::LoadingState::kFinished);

  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kValidCountryCode, 0);
  SetRegion("99.us-intl");
  const CountryId fallback_id = client.GetFallbackCountryId();
  ASSERT_EQ(fallback_id, country_codes::GetCurrentCountryID());
  histogram_tester().ExpectUniqueSample(
      kCrOSMissingVariationData, ChromeOSFallbackCountry::kInvalidCountryCode,
      1);
  histogram_tester().ExpectUniqueSample(kVpdRegionSplittingOutcome, false, 1);
}
#endif

TEST_F(RegionalCapabilitiesServiceClientTest, GetFallbackCountryId) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);

  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
}

#if BUILDFLAG(IS_ANDROID)

TEST_F(RegionalCapabilitiesServiceClientTest, FetchCountryId_Sync) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);

  TestSupportAndroid test_support;
  test_support.ReturnDeviceCountry(kBelgiumCountryCode);

  std::optional<CountryId> actual_country_id;
  client.FetchCountryId(base::BindLambdaForTesting(
      [&actual_country_id](CountryId device_country_id) {
        actual_country_id = device_country_id;
      }));
  EXPECT_EQ(actual_country_id, kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceClientTest, FetchCountryId_Async) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);

  TestSupportAndroid test_support;

  std::optional<CountryId> actual_country_id;
  client.FetchCountryId(base::BindLambdaForTesting(
      [&actual_country_id](CountryId device_country_id) {
        actual_country_id = device_country_id;
      }));
  EXPECT_EQ(actual_country_id, std::nullopt);

  test_support.ReturnDeviceCountry(kBelgiumCountryCode);

  EXPECT_EQ(actual_country_id, kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceClientTest, FetchCountryId_Failure) {
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);

  TestSupportAndroid test_support;
  test_support.TriggerDeviceCountryFailure();

  std::optional<CountryId> actual_country_id;
  client.FetchCountryId(base::BindLambdaForTesting(
      [&actual_country_id](CountryId device_country_id) {
        actual_country_id = device_country_id;
      }));

  // The callback is dropped.
  EXPECT_EQ(actual_country_id, std::nullopt);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace regional_capabilities
