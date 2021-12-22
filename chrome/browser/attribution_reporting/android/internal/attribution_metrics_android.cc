// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/attribution_reporting/android/internal/jni_headers/AttributionMetrics_jni.h"

#include "base/android/jni_string.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"

using base::android::JavaParamRef;

jlong JNI_AttributionMetrics_RecordEnumMetrics(
    JNIEnv* env,
    const JavaParamRef<jstring>& enum_name,
    jlong histogram_ptr,
    jint enum_value,
    jint exclusive_max,
    jint count) {
  base::HistogramBase* histogram = nullptr;
  if (histogram_ptr) {
    histogram = reinterpret_cast<base::HistogramBase*>(histogram_ptr);
  } else {
    // See base::UmaHistogramExactLinear implementation. Since we may be adding
    // a large number of samples at once, we have to get the histogram ourselves
    // and use the AddCount method.
    histogram = base::LinearHistogram::FactoryGet(
        base::android::ConvertJavaStringToUTF8(env, enum_name), 1,
        exclusive_max, exclusive_max + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
  }

  histogram->AddCount(enum_value, count);
  return reinterpret_cast<jlong>(histogram);
}
