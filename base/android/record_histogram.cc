// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_jni_headers/RecordHistogram_jni.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"

namespace base {
namespace android {

using HistogramsSnapshot =
    std::map<std::string, std::unique_ptr<HistogramSamples>>;

// This backs a Java test util for testing histograms -
// MetricsUtils.HistogramDelta. It should live in a test-specific file, but we
// currently can't have test-specific native code packaged in test-specific Java
// targets - see http://crbug.com/415945.
jint JNI_RecordHistogram_GetHistogramValueCountForTesting(
    JNIEnv* env,
    const JavaParamRef<jstring>& histogram_name,
    jint sample,
    jlong snapshot_ptr) {
  std::string name = android::ConvertJavaStringToUTF8(env, histogram_name);
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram (yet?).
    return 0;
  }

  int actual_count = histogram->SnapshotSamples()->GetCount(sample);
  if (snapshot_ptr) {
    auto* snapshot = reinterpret_cast<HistogramsSnapshot*>(snapshot_ptr);
    auto snapshot_data = snapshot->find(name);
    if (snapshot_data != snapshot->end())
      actual_count -= snapshot_data->second->GetCount(sample);
  }

  return actual_count;
}

jint JNI_RecordHistogram_GetHistogramTotalCountForTesting(
    JNIEnv* env,
    const JavaParamRef<jstring>& histogram_name,
    jlong snapshot_ptr) {
  std::string name = android::ConvertJavaStringToUTF8(env, histogram_name);
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram.
    return 0;
  }

  int actual_count = histogram->SnapshotSamples()->TotalCount();
  if (snapshot_ptr) {
    auto* snapshot = reinterpret_cast<HistogramsSnapshot*>(snapshot_ptr);
    auto snapshot_data = snapshot->find(name);
    if (snapshot_data != snapshot->end())
      actual_count -= snapshot_data->second->TotalCount();
  }
  return actual_count;
}

jlong JNI_RecordHistogram_CreateHistogramSnapshotForTesting(JNIEnv* env) {
  HistogramsSnapshot* snapshot = new HistogramsSnapshot();
  for (const auto* const histogram : StatisticsRecorder::GetHistograms()) {
    (*snapshot)[histogram->histogram_name()] = histogram->SnapshotSamples();
  }
  return reinterpret_cast<intptr_t>(snapshot);
}

void JNI_RecordHistogram_DestroyHistogramSnapshotForTesting(
    JNIEnv* env,
    jlong snapshot_ptr) {
  delete reinterpret_cast<HistogramsSnapshot*>(snapshot_ptr);
}

}  // namespace android
}  // namespace base
