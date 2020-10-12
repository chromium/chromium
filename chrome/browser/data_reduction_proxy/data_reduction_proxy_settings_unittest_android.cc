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
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
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

}  // namespace
