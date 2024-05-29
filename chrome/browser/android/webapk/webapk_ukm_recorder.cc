// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_ukm_recorder.h"

#include <jni.h>

#include <algorithm>

#include "base/android/jni_string.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/webapps/browser/android/webapk/webapk_types.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/android/browserservices/metrics/jni_headers/WebApkUkmRecorder_jni.h"

namespace webapk {

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
void WebApkUkmRecorder::RecordInstall(
    const GURL& manifest_id,
    webapps::WebappInstallSource install_source,
    blink::mojom::DisplayMode display) {
  if (!manifest_id.is_valid()) {
    return;
  }

  ukm::SourceId source_id =
      ukm::AppSourceUrlRecorder::GetSourceIdForPWA(manifest_id);

  // All installs through this method are browser-installs (ie, they should all
  // use the "browser" distributor).
  ukm::builders::WebAPK_Install(source_id)
      .SetDistributor(static_cast<int64_t>(webapps::WebApkDistributor::BROWSER))
      .SetInstall(1)
      .SetInstallSource(static_cast<int64_t>(install_source))
      .SetDisplayMode(static_cast<int64_t>(display))
      .Record(ukm::UkmRecorder::Get());
}

// static
void WebApkUkmRecorder::RecordSessionDuration(const GURL& manifest_id,
                                              int64_t distributor,
                                              int64_t version_code,
                                              int64_t duration) {
  if (!manifest_id.is_valid()) {
    return;
  }

  ukm::SourceId source_id =
      ukm::AppSourceUrlRecorder::GetSourceIdForPWA(manifest_id);
  ukm::builders::WebAPK_SessionEnd(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetSessionDuration(ukm::GetExponentialBucketMinForUserTiming(duration))
      .Record(ukm::UkmRecorder::Get());
}

// static
void WebApkUkmRecorder::RecordVisit(const GURL& manifest_id,
                                    int64_t distributor,
                                    int64_t version_code,
                                    int source) {
  if (!manifest_id.is_valid()) {
    return;
  }

  ukm::SourceId source_id =
      ukm::AppSourceUrlRecorder::GetSourceIdForPWA(manifest_id);
  ukm::builders::WebAPK_Visit(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetLaunchSource(source)
      .SetLaunch(1)
      .Record(ukm::UkmRecorder::Get());
}

// static
void WebApkUkmRecorder::RecordUninstall(const GURL& manifest_id,
                                        int64_t distributor,
                                        int64_t version_code,
                                        int64_t launch_count,
                                        int64_t installed_duration_ms) {
  // UKM metric |launch_count| parameter is enum. '2' indicates >= 2 launches.
  launch_count = std::clamp<int64_t>(launch_count, 0, 2);
  ukm::SourceId source_id =
      ukm::AppSourceUrlRecorder::GetSourceIdForPWA(manifest_id);
  ukm::builders::WebAPK_Uninstall(source_id)
      .SetDistributor(distributor)
      .SetAppVersion(version_code)
      .SetLifetimeLaunches(launch_count)
      .SetInstalledDuration(
          ukm::GetExponentialBucketMinForUserTiming(installed_duration_ms))
      .SetUninstall(1)
      .Record(ukm::UkmRecorder::Get());
}

// static
void WebApkUkmRecorder::RecordWebApkableVisit(const GURL& manifest_id) {
  if (!manifest_id.is_valid()) {
    return;
  }

  ukm::SourceId source_id =
      ukm::AppSourceUrlRecorder::GetSourceIdForPWA(manifest_id);
  ukm::builders::PWA_Visit(source_id).SetWebAPKableSiteVisit(1).Record(
      ukm::UkmRecorder::Get());
}

// Called by the Java counterpart to record the Session Duration UKM metric.
void JNI_WebApkUkmRecorder_RecordSessionDuration(
    JNIEnv* env,
    const JavaParamRef<jstring>& manifest_id,
    jint distributor,
    jint version_code,
    jlong duration) {
  WebApkUkmRecorder::RecordSessionDuration(
      ConvertNullableJavaStringToGURL(env, manifest_id), distributor,
      version_code, duration);
}

// Called by the Java counterpart to record the Visit UKM metric.
void JNI_WebApkUkmRecorder_RecordVisit(JNIEnv* env,
                                       const JavaParamRef<jstring>& manifest_id,
                                       jint distributor,
                                       jint version_code,
                                       jint source) {
  WebApkUkmRecorder::RecordVisit(
      ConvertNullableJavaStringToGURL(env, manifest_id), distributor,
      version_code, source);
}

// Called by the Java counterpart to record the Uninstall UKM metrics.
void JNI_WebApkUkmRecorder_RecordUninstall(
    JNIEnv* env,
    const JavaParamRef<jstring>& manifest_id,
    jint distributor,
    jint version_code,
    jint launch_count,
    jlong installed_duration_ms) {
  WebApkUkmRecorder::RecordUninstall(
      ConvertNullableJavaStringToGURL(env, manifest_id), distributor,
      version_code, launch_count, installed_duration_ms);
}
}  // namespace webapk
