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
