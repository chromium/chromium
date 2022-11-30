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
  void Destroy(JNIEnv* env);
  void RegisterOptimizationTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& joptimization_types);
  void CanApplyOptimizationAsync(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_gurl,
      jint optimization_type,
      const base::android::JavaParamRef<jobject>& java_callback);
  void CanApplyOptimization(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_gurl,
      jint optimization_type,
      const base::android::JavaParamRef<jobject>& java_callback);
  void OnNewPushNotification(
      JNIEnv* env,
      const base::android::JavaRef<jbyteArray>& j_encoded_notification);
  void OnDeferredStartup(JNIEnv* env);

 private:
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_
