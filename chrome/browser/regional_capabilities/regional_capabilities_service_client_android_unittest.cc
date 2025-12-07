// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/regional_capabilities/regional_capabilities_service_client_android.h"

#include <optional>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/bind.h"
#include "chrome/browser/regional_capabilities/android/test_utils_jni_headers/RegionalCapabilitiesServiceTestUtil_jni.h"
#include "components/country_codes/country_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace regional_capabilities {
namespace {

using ::country_codes::CountryId;

constexpr char kBelgiumCountryCode[] = "BE";

constexpr CountryId kBelgiumCountryId(kBelgiumCountryCode);

class TestSupportAndroid {
 public:
  TestSupportAndroid() {
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jobject> java_ref =
        Java_RegionalCapabilitiesServiceTestUtil_Constructor(env);
    java_test_util_ref_.Reset(env, java_ref);
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

TEST(RegionalCapabilitiesServiceClientAndroidTest, FetchCountryId_Sync) {
  RegionalCapabilitiesServiceClientAndroid client(
      /*variations_service=*/nullptr);

  TestSupportAndroid test_support;
  test_support.ReturnDeviceCountry(kBelgiumCountryCode);

  std::optional<CountryId> actual_country_id;
  client.FetchCountryId(base::BindLambdaForTesting(
      [&actual_country_id](CountryId device_country_id) {
        actual_country_id = device_country_id;
      }));
  EXPECT_EQ(actual_country_id, kBelgiumCountryId);
}

TEST(RegionalCapabilitiesServiceClientAndroidTest, FetchCountryId_Async) {
  RegionalCapabilitiesServiceClientAndroid client(
      /*variations_service=*/nullptr);

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

TEST(RegionalCapabilitiesServiceClientAndroidTest, FetchCountryId_Failure) {
  RegionalCapabilitiesServiceClientAndroid client(
      /*variations_service=*/nullptr);

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

TEST(RegionalCapabilitiesServiceClientAndroidTest,
     ReturnsDefaultDeviceProgram) {
  RegionalCapabilitiesServiceClientAndroid client(
      /*variations_service=*/nullptr);
  EXPECT_EQ(client.GetDeviceProgram(), Program::kDefault);
}

}  // namespace
}  // namespace regional_capabilities

DEFINE_JNI(RegionalCapabilitiesServiceTestUtil)
