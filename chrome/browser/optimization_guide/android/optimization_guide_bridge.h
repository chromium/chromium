// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"

class OptimizationGuideKeyedService;

namespace optimization_guide {
namespace android {

// The C++ counterpart to Java's OptimizationGuideBridge. Together, these
// classes expose OptimizationGuideKeyedService to Java.
class OptimizationGuideBridge {
 public:
  explicit OptimizationGuideBridge(
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  OptimizationGuideBridge(const OptimizationGuideBridge&) = delete;
  OptimizationGuideBridge& operator=(const OptimizationGuideBridge&) = delete;
  void Destroy(JNIEnv* env);
  void RegisterOptimizationTypes(
      JNIEnv* env,
      const base::android::JavaParamRef<jintArray>& joptimization_types);
  void CanApplyOptimization(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& java_gurl,
      jint optimization_type,
      const base::android::JavaParamRef<jobject>& java_callback);

 private:
  OptimizationGuideKeyedService* optimization_guide_keyed_service_;
};

}  // namespace android
}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_ANDROID_OPTIMIZATION_GUIDE_BRIDGE_H_
