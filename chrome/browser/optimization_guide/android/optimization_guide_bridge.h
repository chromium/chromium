// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_

#include <jni.h>

#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/push_notification.pb.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/gurl.h"

class OptimizationGuideKeyedService;

namespace optimization_guide {
namespace android {

// The C++ counterpart to Java's OptimizationGuideBridge. Together, these
// classes expose OptimizationGuideKeyedService to Java.
class OptimizationGuideBridge {
 public:
  static std::vector<proto::HintNotificationPayload> GetCachedNotifications(
      proto::OptimizationType opt_type);
  static base::flat_set<proto::OptimizationType>
  GetOptTypesWithPushNotifications();
  static base::flat_set<proto::OptimizationType>
  GetOptTypesThatOverflowedPushNotifications();
  static void ClearCacheForOptimizationType(proto::OptimizationType opt_type);
  static void OnNotificationNotHandledByNative(
      proto::HintNotificationPayload notification);

  explicit OptimizationGuideBridge(
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  OptimizationGuideBridge(const OptimizationGuideBridge&) = delete;
  OptimizationGuideBridge& operator=(const OptimizationGuideBridge&) = delete;
  ~OptimizationGuideBridge();

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void RegisterOptimizationTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& joptimization_types);
  void CanApplyOptimization(
      JNIEnv* env,
      GURL& url,
      jint optimization_type,
      const base::android::JavaParamRef<jobject>& java_callback);
  void CanApplyOptimizationOnDemand(
      JNIEnv* env,
      std::vector<GURL>& urls,
      const base::android::JavaParamRef<jintArray>& joptimization_types,
      jint request_context,
      const base::android::JavaParamRef<jobject>& java_callback,
      jni_zero::ByteArrayView& request_context_metadata_serialized);
  void OnNewPushNotification(
      JNIEnv* env,
      const base::android::JavaRef<jbyteArray>& j_encoded_notification);
  void OnDeferredStartup(JNIEnv* env);

 private:
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_
