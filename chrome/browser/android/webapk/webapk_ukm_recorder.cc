// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/numerics/ranges.h"
#include "chrome/android/chrome_jni_headers/WebApkUkmRecorder_jni.h"
#include "chrome/browser/android/webapk/webapk_types.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

using base::android::JavaParamRef;

namespace {

// Converts Java string to GURL. Returns an empty GURL if the Java string is
// null.
GURL ConvertNullableJavaStringToGURL(JNIEnv* env,
                                     const JavaParamRef<jstring>& java_url) {
  return java_url ? GURL(base::android::ConvertJavaStringToUTF8(env, java_url))
                  : GURL();
}

}  // namespace

// static
void WebApkUkmRecorder::RecordInstall(const GURL& manifest_url,
                                      int version_code) {
  if (!manifest_url.is_valid())
    return;

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, manifest_url);

  // All installs through this method are browser-installs (ie, they should all
  // use the "browser" distributor).
  ukm::builders::WebAPK_Install(source_id)
      .SetDistributor(static_cast<int64_t>(WebApkDistributor::BROWSER))
      .SetAppVersion(version_code)
      .SetInstall(1)
      .Record(ukm_recorder);
}

// static
void WebApkUkmRecorder::RecordSessionDuration(const GURL& manifest_url,
                                              int64_t distributor,
                                              int64_t version_code,
                                              int64_t duration) {
  if (!manifest_url.is_valid())
    return;

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, manifest_url);
  ukm::builders::WebAPK_SessionEnd(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetSessionDuration(ukm::GetExponentialBucketMinForUserTiming(duration))
      .Record(ukm_recorder);
}

// static
void WebApkUkmRecorder::RecordVisit(const GURL& manifest_url,
                                    int64_t distributor,
                                    int64_t version_code,
                                    int source) {
  if (!manifest_url.is_valid())
    return;

  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, manifest_url);
  ukm::builders::WebAPK_Visit(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetLaunchSource(source)
      .SetLaunch(1)
      .Record(ukm_recorder);
}

// static
void WebApkUkmRecorder::RecordUninstall(const GURL& manifest_url,
                                        int64_t distributor,
                                        int64_t version_code,
                                        int64_t launch_count,
                                        int64_t installed_duration_ms) {
  // UKM metric |launch_count| parameter is enum. '2' indicates >= 2 launches.
  launch_count = base::ClampToRange<int64_t>(launch_count, 0, 2);
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm_recorder->UpdateSourceURL(source_id, manifest_url);
  ukm::builders::WebAPK_Uninstall(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetLifetimeLaunches(launch_count)
      .SetInstalledDuration(
          ukm::GetExponentialBucketMinForUserTiming(installed_duration_ms))
      .SetUninstall(1)
      .Record(ukm_recorder);
}

// Called by the Java counterpart to record the Session Duration UKM metric.
void JNI_WebApkUkmRecorder_RecordSessionDuration(
    JNIEnv* env,
    const JavaParamRef<jstring>& manifest_url,
    jint distributor,
    jint version_code,
    jlong duration) {
  WebApkUkmRecorder::RecordSessionDuration(
      ConvertNullableJavaStringToGURL(env, manifest_url), distributor,
      version_code, duration);
}

// Called by the Java counterpart to record the Visit UKM metric.
void JNI_WebApkUkmRecorder_RecordVisit(
    JNIEnv* env,
    const JavaParamRef<jstring>& manifest_url,
    jint distributor,
    jint version_code,
    jint source) {
  WebApkUkmRecorder::RecordVisit(
      ConvertNullableJavaStringToGURL(env, manifest_url), distributor,
      version_code, source);
}

// Called by the Java counterpart to record the Uninstall UKM metrics.
void JNI_WebApkUkmRecorder_RecordUninstall(
    JNIEnv* env,
    const JavaParamRef<jstring>& manifest_url,
    jint distributor,
    jint version_code,
    jint launch_count,
    jlong installed_duration_ms) {
  WebApkUkmRecorder::RecordUninstall(
      ConvertNullableJavaStringToGURL(env, manifest_url), distributor,
      version_code, launch_count, installed_duration_ms);
}
