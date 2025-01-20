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

namespace regional_capabilities {

namespace {

#if BUILDFLAG(IS_ANDROID)
constexpr char kBelgiumCountryCode[] = "BE";

constexpr int kBelgiumCountryId =
    country_codes::CountryCharsToCountryID(kBelgiumCountryCode[0],
                                           kBelgiumCountryCode[1]);

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

class RegionalCapabilitiesServiceClientTest : public ::testing::Test {};

TEST_F(RegionalCapabilitiesServiceClientTest, GetFallbackCountryId) {
#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
  RegionalCapabilitiesServiceClient client(/* variations_service= */ nullptr);
#else
  RegionalCapabilitiesServiceClient client;
#endif

  EXPECT_EQ(client.GetFallbackCountryId(),
            country_codes::GetCurrentCountryID());
}

#if BUILDFLAG(IS_ANDROID)

TEST_F(RegionalCapabilitiesServiceClientTest, FetchCountryId_Sync) {
  RegionalCapabilitiesServiceClient client;

  TestSupportAndroid test_support;
  test_support.ReturnDeviceCountry(kBelgiumCountryCode);

  std::optional<int> actual_country_id;
  client.FetchCountryId(
      base::BindLambdaForTesting([&actual_country_id](int device_country_id) {
        actual_country_id = device_country_id;
      }));
  EXPECT_EQ(actual_country_id, kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceClientTest, FetchCountryId_Async) {
  RegionalCapabilitiesServiceClient client;

  TestSupportAndroid test_support;

  std::optional<int> actual_country_id;
  client.FetchCountryId(
      base::BindLambdaForTesting([&actual_country_id](int device_country_id) {
        actual_country_id = device_country_id;
      }));
  EXPECT_EQ(actual_country_id, std::nullopt);

  test_support.ReturnDeviceCountry(kBelgiumCountryCode);

  EXPECT_EQ(actual_country_id, kBelgiumCountryId);
}

TEST_F(RegionalCapabilitiesServiceClientTest, FetchCountryId_Failure) {
  RegionalCapabilitiesServiceClient client;

  TestSupportAndroid test_support;
  test_support.TriggerDeviceCountryFailure();

  std::optional<int> actual_country_id;
  client.FetchCountryId(
      base::BindLambdaForTesting([&actual_country_id](int device_country_id) {
        actual_country_id = device_country_id;
      }));

  // The callback is dropped.
  EXPECT_EQ(actual_country_id, std::nullopt);
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace regional_capabilities
