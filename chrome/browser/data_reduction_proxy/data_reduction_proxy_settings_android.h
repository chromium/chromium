// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
#define CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_

#include <memory>
#include <vector>

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/data_reduction_proxy/proto/data_store.pb.h"
#include "components/prefs/pref_member.h"

class Profile;

namespace data_reduction_proxy {
class DataReductionProxySettings;
}

// Central point for configuring the data reduction proxy on Android.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettingsAndroid {
 public:
  DataReductionProxySettingsAndroid(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  virtual ~DataReductionProxySettingsAndroid();

  void InitDataReductionProxySettings(Profile* profile);

  // JNI wrapper interfaces to the indentically-named superclass methods.
  jboolean IsDataReductionProxyPromoAllowed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsDataReductionProxyFREPromoAllowed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsDataReductionProxyEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean IsDataReductionProxyManaged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetDataReductionProxyEnabled(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean enabled);
  jlong GetDataReductionLastUpdateTime(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void ClearDataSavingStatistics(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint reason);
  jlong GetTotalHttpContentLengthSaved(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jlongArray> GetDailyOriginalContentLengths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jlongArray> GetDailyReceivedContentLengths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jstring>
  GetDataReductionProxyPassThroughHeader(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Return a Java |ContentLengths| object wrapping the results of a call to
  // DataReductionProxySettings::GetContentLengths.
  base::android::ScopedJavaLocalRef<jobject> GetContentLengths(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Determines whether the data reduction proxy is unreachable. This is
  // done by keeping a count of requests which go through proxy vs those
  // which should have gone through the proxy based on the config.
  jboolean IsDataReductionProxyUnreachable(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jstring> MaybeRewriteWebliteUrl(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& obj,
      const base::android::JavaRef<jstring>& url);

  base::android::ScopedJavaLocalRef<jstring> GetTokenForAuthChallenge(
      JNIEnv* env,
      jobject obj,
      jstring host,
      jstring realm);

  // Gets the historical data usage for |numDays| and adds them to a list that
  // groups data use by hostname.
  void QueryDataUsage(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      const base::android::JavaParamRef<jobject>& j_result_obj,
                      jint num_days);
  void OnQueryDataUsageComplete(
      JavaObjectWeakGlobalRef obj,
      const base::android::ScopedJavaGlobalRef<jobject>& j_result_obj,
      jint num_days,
      std::unique_ptr<std::vector<data_reduction_proxy::DataUsageBucket>>
          data_usage);

 private:
  friend class TestDataReductionProxySettingsAndroid;

  // For testing purposes.
  DataReductionProxySettingsAndroid();

  base::android::ScopedJavaLocalRef<jlongArray> GetDailyContentLengths(
      JNIEnv* env,
      const char* pref_name);

  virtual data_reduction_proxy::DataReductionProxySettings* Settings();

  base::WeakPtrFactory<DataReductionProxySettingsAndroid> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettingsAndroid);
};

#endif  // CHROME_BROWSER_DATA_REDUCTION_PROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
