// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <map>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base_jni_headers/RecordHistogram_jni.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {
namespace android {
namespace {

// Simple thread-safe wrapper for caching histograms. This avoids
// relatively expensive JNI string translation for each recording.
class HistogramCache {
 public:
  HistogramCache() {}

  std::string HistogramConstructionParamsToString(HistogramBase* histogram) {
    std::string params_str = histogram->histogram_name();
    switch (histogram->GetHistogramType()) {
      case HISTOGRAM:
      case LINEAR_HISTOGRAM:
      case BOOLEAN_HISTOGRAM:
      case CUSTOM_HISTOGRAM: {
        Histogram* hist = static_cast<Histogram*>(histogram);
        params_str += StringPrintf("/%d/%d/%d", hist->declared_min(),
                                   hist->declared_max(), hist->bucket_count());
        break;
      }
      case SPARSE_HISTOGRAM:
      case DUMMY_HISTOGRAM:
        break;
    }
    return params_str;
  }

  void JNI_RecordHistogram_CheckHistogramArgs(JNIEnv* env,
                                              jstring j_histogram_name,
                                              int32_t expected_min,
                                              int32_t expected_max,
                                              uint32_t expected_bucket_count,
                                              HistogramBase* histogram) {
    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    bool valid_arguments = Histogram::InspectConstructionArguments(
        histogram_name, &expected_min, &expected_max, &expected_bucket_count);
    DCHECK(valid_arguments);
    DCHECK(histogram->HasConstructionArguments(expected_min, expected_max,
                                               expected_bucket_count))
        << histogram_name << "/" << expected_min << "/" << expected_max << "/"
        << expected_bucket_count << " vs. "
        << HistogramConstructionParamsToString(histogram);
  }

  HistogramBase* JNI_RecordHistogram_BooleanHistogram(JNIEnv* env,
                                                      jstring j_histogram_name,
                                                      jlong j_histogram_key) {
    DCHECK(j_histogram_name);
    HistogramBase* histogram = HistogramFromKey(j_histogram_key);
    if (histogram)
      return histogram;

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    histogram = BooleanHistogram::FactoryGet(
        histogram_name, HistogramBase::kUmaTargetedHistogramFlag);
    return histogram;
  }

  HistogramBase* JNI_RecordHistogram_EnumeratedHistogram(
      JNIEnv* env,
      jstring j_histogram_name,
      jlong j_histogram_key,
      jint j_boundary) {
    DCHECK(j_histogram_name);
    HistogramBase* histogram = HistogramFromKey(j_histogram_key);
    int32_t boundary = static_cast<int32_t>(j_boundary);
    if (histogram) {
      JNI_RecordHistogram_CheckHistogramArgs(env, j_histogram_name, 1, boundary,
                                             boundary + 1, histogram);
      return histogram;
    }

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    histogram =
        LinearHistogram::FactoryGet(histogram_name, 1, boundary, boundary + 1,
                                    HistogramBase::kUmaTargetedHistogramFlag);
    return histogram;
  }

  HistogramBase* JNI_RecordHistogram_CustomCountHistogram(
      JNIEnv* env,
      jstring j_histogram_name,
      jlong j_histogram_key,
      jint j_min,
      jint j_max,
      jint j_num_buckets) {
    DCHECK(j_histogram_name);
    int32_t min = static_cast<int32_t>(j_min);
    int32_t max = static_cast<int32_t>(j_max);
    int32_t num_buckets = static_cast<int32_t>(j_num_buckets);
    HistogramBase* histogram = HistogramFromKey(j_histogram_key);
    if (histogram) {
      JNI_RecordHistogram_CheckHistogramArgs(env, j_histogram_name, min, max,
                                             num_buckets, histogram);
      return histogram;
    }

    DCHECK_GE(min, 1) << "The min expected sample must be >= 1";

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    histogram =
        Histogram::FactoryGet(histogram_name, min, max, num_buckets,
                              HistogramBase::kUmaTargetedHistogramFlag);
    return histogram;
  }

  HistogramBase* JNI_RecordHistogram_LinearCountHistogram(
      JNIEnv* env,
      jstring j_histogram_name,
      jlong j_histogram_key,
      jint j_min,
      jint j_max,
      jint j_num_buckets) {
    DCHECK(j_histogram_name);
    int32_t min = static_cast<int32_t>(j_min);
    int32_t max = static_cast<int32_t>(j_max);
    int32_t num_buckets = static_cast<int32_t>(j_num_buckets);
    HistogramBase* histogram = HistogramFromKey(j_histogram_key);
    if (histogram) {
      JNI_RecordHistogram_CheckHistogramArgs(env, j_histogram_name, min, max,
                                             num_buckets, histogram);
      return histogram;
    }

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    histogram =
        LinearHistogram::FactoryGet(histogram_name, min, max, num_buckets,
                                    HistogramBase::kUmaTargetedHistogramFlag);
    return histogram;
  }

  HistogramBase* JNI_RecordHistogram_SparseHistogram(JNIEnv* env,
                                                     jstring j_histogram_name,
                                                     jlong j_histogram_key) {
    DCHECK(j_histogram_name);
    HistogramBase* histogram = HistogramFromKey(j_histogram_key);
    if (histogram)
      return histogram;

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    histogram = SparseHistogram::FactoryGet(
        histogram_name, HistogramBase::kUmaTargetedHistogramFlag);
    return histogram;
  }

  HistogramBase* JNI_RecordHistogram_CustomTimesHistogram(
      JNIEnv* env,
      jstring j_histogram_name,
      jlong j_histogram_key,
      jint j_min,
      jint j_max,
      jint j_bucket_count) {
    DCHECK(j_histogram_name);
    HistogramBase* histogram = HistogramFromKey(j_histogram_key);
    int32_t min = static_cast<int32_t>(j_min);
    int32_t max = static_cast<int32_t>(j_max);
    int32_t bucket_count = static_cast<int32_t>(j_bucket_count);
    if (histogram) {
      JNI_RecordHistogram_CheckHistogramArgs(env, j_histogram_name, min, max,
                                             bucket_count, histogram);
      return histogram;
    }

    std::string histogram_name = ConvertJavaStringToUTF8(env, j_histogram_name);
    // This intentionally uses FactoryGet and not FactoryTimeGet. FactoryTimeGet
    // is just a convenience for constructing the underlying Histogram with
    // TimeDelta arguments.
    histogram = Histogram::FactoryGet(histogram_name, min, max, bucket_count,
                                      HistogramBase::kUmaTargetedHistogramFlag);
    return histogram;
  }

 private:
  // Convert a jlong |histogram_key| from Java to a HistogramBase* via a cast.
  // The Java side caches these in a map (see RecordHistogram.java), which is
  // safe to do since C++ Histogram objects are never freed.
  static HistogramBase* HistogramFromKey(jlong j_histogram_key) {
    return reinterpret_cast<HistogramBase*>(j_histogram_key);
  }

  DISALLOW_COPY_AND_ASSIGN(HistogramCache);
};

LazyInstance<HistogramCache>::Leaky g_histograms;

}  // namespace

jlong JNI_RecordHistogram_RecordBooleanHistogram(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_histogram_name,
    jlong j_histogram_key,
    jboolean j_sample) {
  bool sample = static_cast<bool>(j_sample);
  HistogramBase* histogram =
      g_histograms.Get().JNI_RecordHistogram_BooleanHistogram(
          env, j_histogram_name, j_histogram_key);
  histogram->AddBoolean(sample);
  return reinterpret_cast<jlong>(histogram);
}

jlong JNI_RecordHistogram_RecordEnumeratedHistogram(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_histogram_name,
    jlong j_histogram_key,
    jint j_sample,
    jint j_boundary) {
  int sample = static_cast<int>(j_sample);

  HistogramBase* histogram =
      g_histograms.Get().JNI_RecordHistogram_EnumeratedHistogram(
          env, j_histogram_name, j_histogram_key, j_boundary);
  histogram->Add(sample);
  return reinterpret_cast<jlong>(histogram);
}

jlong JNI_RecordHistogram_RecordCustomCountHistogram(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_histogram_name,
    jlong j_histogram_key,
    jint j_sample,
    jint j_min,
    jint j_max,
    jint j_num_buckets) {
  int sample = static_cast<int>(j_sample);

  HistogramBase* histogram =
      g_histograms.Get().JNI_RecordHistogram_CustomCountHistogram(
          env, j_histogram_name, j_histogram_key, j_min, j_max, j_num_buckets);
  histogram->Add(sample);
  return reinterpret_cast<jlong>(histogram);
}

jlong JNI_RecordHistogram_RecordLinearCountHistogram(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_histogram_name,
    jlong j_histogram_key,
    jint j_sample,
    jint j_min,
    jint j_max,
    jint j_num_buckets) {
  int sample = static_cast<int>(j_sample);

  HistogramBase* histogram =
      g_histograms.Get().JNI_RecordHistogram_LinearCountHistogram(
          env, j_histogram_name, j_histogram_key, j_min, j_max, j_num_buckets);
  histogram->Add(sample);
  return reinterpret_cast<jlong>(histogram);
}

jlong JNI_RecordHistogram_RecordSparseHistogram(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_histogram_name,
    jlong j_histogram_key,
    jint j_sample) {
  int sample = static_cast<int>(j_sample);
  HistogramBase* histogram =
      g_histograms.Get().JNI_RecordHistogram_SparseHistogram(
          env, j_histogram_name, j_histogram_key);
  histogram->Add(sample);
  return reinterpret_cast<jlong>(histogram);
}

jlong JNI_RecordHistogram_RecordCustomTimesHistogramMilliseconds(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_histogram_name,
    jlong j_histogram_key,
    jint j_duration,
    jint j_min,
    jint j_max,
    jint j_num_buckets) {
  HistogramBase* histogram =
      g_histograms.Get().JNI_RecordHistogram_CustomTimesHistogram(
          env, j_histogram_name, j_histogram_key, j_min, j_max, j_num_buckets);
  histogram->AddTime(
      TimeDelta::FromMilliseconds(static_cast<int64_t>(j_duration)));
  return reinterpret_cast<jlong>(histogram);
}

// This backs a Java test util for testing histograms -
// MetricsUtils.HistogramDelta. It should live in a test-specific file, but we
// currently can't have test-specific native code packaged in test-specific Java
// targets - see http://crbug.com/415945.
jint JNI_RecordHistogram_GetHistogramValueCountForTesting(
    JNIEnv* env,
    const JavaParamRef<jstring>& histogram_name,
    jint sample) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(
      android::ConvertJavaStringToUTF8(env, histogram_name));
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram (yet?).
    return 0;
  }

  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  return samples->GetCount(static_cast<int>(sample));
}

jint JNI_RecordHistogram_GetHistogramTotalCountForTesting(
    JNIEnv* env,
    const JavaParamRef<jstring>& histogram_name) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(
      android::ConvertJavaStringToUTF8(env, histogram_name));
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram.
    return 0;
  }

  return histogram->SnapshotSamples()->TotalCount();
}

}  // namespace android
}  // namespace base
