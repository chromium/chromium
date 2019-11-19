// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_settings_android.h"

#include <stddef.h>

#include <map>
#include <memory>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/base64.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/proxy_server.h"
#include "net/socket/socket_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace base {
class Clock;
}

using testing::_;
using testing::AnyNumber;
using testing::Return;

using data_reduction_proxy::DataReductionProxySettings;

// Used for testing the DataReductionProxySettingsAndroid class.
class TestDataReductionProxySettingsAndroid
    : public DataReductionProxySettingsAndroid {
 public:
  // Constructs an Android settings object for test that wraps the provided
  // settings object.
  explicit TestDataReductionProxySettingsAndroid(
      DataReductionProxySettings* settings)
      : DataReductionProxySettingsAndroid(), settings_(settings) {}

  // Returns the provided setting object. Used by wrapping methods.
  DataReductionProxySettings* Settings() override { return settings_; }

  // The wrapped settings object.
  DataReductionProxySettings* settings_;
};

template <class C>
void data_reduction_proxy::DataReductionProxySettingsTestBase::ResetSettings(
    base::Clock* clock) {
  MockDataReductionProxySettings<C>* settings =
      new MockDataReductionProxySettings<C>();
  if (settings_) {
    settings->data_reduction_proxy_service_ =
        std::move(settings_->data_reduction_proxy_service_);
  } else {
    settings->data_reduction_proxy_service_ = test_context_->TakeService();
  }
  settings->data_reduction_proxy_service_->SetSettingsForTesting(settings);
  settings->config_ = test_context_->config();
  test_context_->config()->ResetParamFlagsForTest();
  EXPECT_CALL(*settings, GetOriginalProfilePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  EXPECT_CALL(*settings, GetLocalStatePrefs())
      .Times(AnyNumber())
      .WillRepeatedly(Return(test_context_->pref_service()));
  settings_.reset(settings);
}

template void data_reduction_proxy::DataReductionProxySettingsTestBase::
    ResetSettings<DataReductionProxyChromeSettings>(base::Clock* clock);

namespace {

class DataReductionProxyMockSettingsAndroidTest
    : public data_reduction_proxy::ConcreteDataReductionProxySettingsTest<
          DataReductionProxyChromeSettings> {
 public:
  // DataReductionProxySettingsTest implementation:
  void SetUp() override {
    env_ = base::android::AttachCurrentThread();
    DataReductionProxySettingsTestBase::SetUp();
    ResetSettingsAndroid();
  }

  void ResetSettingsAndroid() {
    settings_android_.reset(
        new TestDataReductionProxySettingsAndroid(settings_.get()));
  }

  DataReductionProxySettings* Settings() { return settings_.get(); }

  DataReductionProxySettingsAndroid* SettingsAndroid() {
    return settings_android_.get();
  }

  std::unique_ptr<DataReductionProxySettingsAndroid> settings_android_;
  JNIEnv* env_;
};

TEST_F(DataReductionProxyMockSettingsAndroidTest,
       TestGetDailyOriginalContentLengths) {
  base::android::ScopedJavaLocalRef<jlongArray> result =
      SettingsAndroid()->GetDailyOriginalContentLengths(env_, nullptr);
  ASSERT_FALSE(result.is_null());

  std::vector<int64_t> result_vector;
  base::android::JavaLongArrayToInt64Vector(env_, result, &result_vector);

  std::vector<int64_t> expected_vector;
  for (size_t i = data_reduction_proxy::kNumDaysInHistory; i;)
    expected_vector.push_back(2 * static_cast<int64_t>(--i));

  EXPECT_EQ(expected_vector, result_vector);
}

TEST_F(DataReductionProxyMockSettingsAndroidTest,
       TestGetDailyReceivedContentLengths) {
  base::android::ScopedJavaLocalRef<jlongArray> result =
      SettingsAndroid()->GetDailyReceivedContentLengths(env_, nullptr);
  ASSERT_FALSE(result.is_null());

  std::vector<int64_t> result_vector;
  base::android::JavaLongArrayToInt64Vector(env_, result, &result_vector);

  std::vector<int64_t> expected_vector;
  for (size_t i = data_reduction_proxy::kNumDaysInHistory; i;)
    expected_vector.push_back(static_cast<int64_t>(--i));

  EXPECT_EQ(expected_vector, result_vector);
}

class DataReductionProxySettingsAndroidTest : public ::testing::Test {
 public:
  DataReductionProxySettingsAndroidTest()
      : env_(base::android::AttachCurrentThread()) {}

  void Init() {
    drp_test_context_ =
        data_reduction_proxy::DataReductionProxyTestContext::Builder()
            .Build();

    drp_test_context_->DisableWarmupURLFetch();

    android_settings_.reset(new TestDataReductionProxySettingsAndroid(
        drp_test_context_->settings()));
  }

  JNIEnv* env() { return env_; }
  data_reduction_proxy::DataReductionProxyTestContext* drp_test_context() {
    return drp_test_context_.get();
  }
  TestDataReductionProxySettingsAndroid* android_settings() {
    return android_settings_.get();
  }

  std::string MaybeRewriteWebliteUrlAsUTF8(base::StringPiece url) {
    return base::android::ConvertJavaStringToUTF8(
        android_settings_->MaybeRewriteWebliteUrl(
            env_, nullptr, base::android::ConvertUTF8ToJavaString(env_, url)));
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  JNIEnv* env_;
  net::MockClientSocketFactory mock_socket_factory_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyTestContext>
      drp_test_context_;
  std::unique_ptr<TestDataReductionProxySettingsAndroid> android_settings_;
};

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlDefaultParams_Http) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyDecidesTransform);
  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));
  EXPECT_EQ("http://example.com/foo",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=http://example.com/foo"));
  EXPECT_EQ("http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/&foo=bar"));

  EXPECT_TRUE(android_settings()
                  ->MaybeRewriteWebliteUrl(env(), nullptr, nullptr)
                  .is_null());

  EXPECT_EQ("not a url", MaybeRewriteWebliteUrlAsUTF8("not a url"));
  EXPECT_EQ("", MaybeRewriteWebliteUrlAsUTF8(""));
  EXPECT_EQ("http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8("http://example.com/"));

  EXPECT_EQ("http://otherhost.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://otherhost.com/i?u=http://example.com/"));

  EXPECT_EQ("http://googleweblight.com/otherpath?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/otherpath?u=http://example.com/"));

  EXPECT_EQ("http://googleweblight.com/i?otherparam=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?otherparam=http://example.com/"));

  // A weblite URL that wraps an invalid URL should not be rewritten.
  EXPECT_EQ(
      "http://googleweblight.com/i?u=notaurl",
      MaybeRewriteWebliteUrlAsUTF8("http://googleweblight.com/i?u=notaurl"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlDefaultParams_Https) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyDecidesTransform);
  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=https://example.com/"));
  EXPECT_EQ("https://example.com/foo",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=https://example.com/foo"));
  EXPECT_EQ("https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=https://example.com/&foo=bar"));

  EXPECT_TRUE(android_settings()
                  ->MaybeRewriteWebliteUrl(env(), nullptr, nullptr)
                  .is_null());

  EXPECT_EQ("not a url", MaybeRewriteWebliteUrlAsUTF8("not a url"));
  EXPECT_EQ("", MaybeRewriteWebliteUrlAsUTF8(""));
  EXPECT_EQ("https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8("https://example.com/"));

  EXPECT_EQ("https://otherhost.com/i?u=https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://otherhost.com/i?u=https://example.com/"));

  EXPECT_EQ("https://googleweblight.com/otherpath?u=https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/otherpath?u=https://example.com/"));

  EXPECT_EQ(
      "https://googleweblight.com/i?otherparam=https://example.com/",
      MaybeRewriteWebliteUrlAsUTF8(
          "https://googleweblight.com/i?otherparam=https://example.com/"));

  EXPECT_EQ(
      "https://googleweblight.com/i?u=notaurl",
      MaybeRewriteWebliteUrlAsUTF8("https://googleweblight.com/i?u=notaurl"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlWithDRPDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      data_reduction_proxy::features::kDataReductionProxyDecidesTransform);
  Init();

  EXPECT_EQ("http://googleweblight.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));

  EXPECT_EQ("https://googleweblight.com/i?u=https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=https://example.com/"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlWithWebliteDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitFromCommandLine(
      "Previews" /* enable_features */,
      "DataReductionProxyDecidesTransform" /* disable_features */);
  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("http://googleweblight.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));

  // DataReductionProxyDecidesTransform should not affect https webpages.
  EXPECT_EQ("https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=https://example.com/"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlWithPreviewsDisabled) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitFromCommandLine(
      "DataReductionProxyDecidesTransform" /* enable_features */,
      "Previews" /* disable_features */);
  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("http://googleweblight.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlWithHoldbackEnabled) {
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));

  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("http://googleweblight.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));

  // DataCompressionProxyHoldback should not affect https webpages.
  EXPECT_EQ("https://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "https://googleweblight.com/i?u=https://example.com/"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlWithCustomParams) {
  base::test::ScopedFeatureList scoped_list;
  std::map<std::string, std::string> params;
  params["weblite_url_host_and_path"] = "testhost.net/testpath";
  params["weblite_url_query_param"] = "testparam";
  scoped_list.InitAndEnableFeatureWithParameters(
      data_reduction_proxy::features::kDataReductionProxyDecidesTransform,
      params);
  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://testhost.net/testpath?testparam=http://example.com/"));

  EXPECT_EQ("http://googleweblight.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));

  EXPECT_EQ("http://otherhost.net/testpath?testparam=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://otherhost.net/testpath?testparam=http://example.com/"));

  EXPECT_EQ("http://testhost.net/otherpath?testparam=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://testhost.net/otherpath?testparam=http://example.com/"));

  EXPECT_EQ("http://testhost.net/testpath?otherparam=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://testhost.net/testpath?otherparam=http://example.com/"));
}

TEST_F(DataReductionProxySettingsAndroidTest,
       MaybeRewriteWebliteUrlWithCustomLegacyParams) {
  base::test::ScopedFeatureList scoped_list;
  std::map<std::string, std::string> params;
  params["weblite_url_host_and_path"] = "googleweblight.com/";
  params["weblite_url_query_param"] = "lite_url";
  scoped_list.InitAndEnableFeatureWithParameters(
      data_reduction_proxy::features::kDataReductionProxyDecidesTransform,
      params);
  Init();
  drp_test_context()->EnableDataReductionProxyWithSecureProxyCheckSuccess();

  EXPECT_EQ("http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/?lite_url=http://example.com/"));

  EXPECT_EQ("http://googleweblight.com/i?u=http://example.com/",
            MaybeRewriteWebliteUrlAsUTF8(
                "http://googleweblight.com/i?u=http://example.com/"));
}

}  // namespace
