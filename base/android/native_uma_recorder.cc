// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/map_util.h"
#include "base/format_macros.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/metrics_jni/NativeUmaRecorder_jni.h"

namespace base {
namespace android {

namespace {

using HistogramsSnapshot =
    std::map<std::string, std::unique_ptr<HistogramSamples>, std::less<>>;

std::string HistogramConstructionParamsToString(HistogramBase* histogram) {
  std::string_view name = histogram->histogram_name();
  switch (histogram->GetHistogramType()) {
    case HISTOGRAM:
    case LINEAR_HISTOGRAM:
    case BOOLEAN_HISTOGRAM:
    case CUSTOM_HISTOGRAM: {
      Histogram* hist = static_cast<Histogram*>(histogram);
      return StringPrintf("%.*s/%d/%d/%" PRIuS, name.length(), name.data(),
                          hist->declared_min(), hist->declared_max(),
                          hist->bucket_count());
    }
    case SPARSE_HISTOGRAM:
    case DUMMY_HISTOGRAM:
      break;
  }
  return std::string(name);
}

// Convert a int64_t |histogram_hint| from Java to a HistogramBase* via a cast.
// The Java side caches these in a map (see NativeUmaRecorder.java), which is
// safe to do since C++ Histogram objects are never freed.
static HistogramBase* HistogramFromHint(int64_t j_histogram_hint) {
  return reinterpret_cast<HistogramBase*>(j_histogram_hint);
}

void CheckHistogramArgs(JNIEnv* env,
                        const std::string& histogram_name,
                        int32_t expected_min,
                        int32_t expected_max,
                        size_t expected_bucket_count,
                        HistogramBase* histogram) {
  const auto validity = Histogram::InspectConstructionArguments(
      histogram_name, &expected_min, &expected_max, &expected_bucket_count);
  DCHECK_EQ(validity, Histogram::kOK);
  DCHECK(histogram->HasConstructionArguments(expected_min, expected_max,
                                             expected_bucket_count))
      << histogram_name << "/" << expected_min << "/" << expected_max << "/"
      << expected_bucket_count << " vs. "
      << HistogramConstructionParamsToString(histogram);
}

HistogramBase* BooleanHistogram(JNIEnv* env,
                                const std::string& histogram_name,
                                int64_t j_histogram_hint) {
  HistogramBase* histogram = HistogramFromHint(j_histogram_hint);
  if (histogram) {
    return histogram;
  }

  histogram = BooleanHistogram::FactoryGet(
      histogram_name, HistogramBase::kUmaTargetedHistogramFlag);
  return histogram;
}

HistogramBase* ExponentialHistogram(JNIEnv* env,
                                    const std::string& histogram_name,
                                    int64_t j_histogram_hint,
                                    int32_t j_min,
                                    int32_t j_max,
                                    int32_t j_num_buckets) {
  int32_t min = j_min;
  int32_t max = j_max;
  size_t num_buckets = static_cast<size_t>(j_num_buckets);
  HistogramBase* histogram = HistogramFromHint(j_histogram_hint);
  if (histogram) {
    CheckHistogramArgs(env, histogram_name, min, max, num_buckets, histogram);
    return histogram;
  }

  DCHECK_GE(min, 1) << "The min expected sample must be >= 1";

  histogram = Histogram::FactoryGet(histogram_name, min, max, num_buckets,
                                    HistogramBase::kUmaTargetedHistogramFlag);
  return histogram;
}

HistogramBase* LinearHistogram(JNIEnv* env,
                               const std::string& j_histogram_name,
                               int64_t j_histogram_hint,
                               int32_t j_min,
                               int32_t j_max,
                               int32_t j_num_buckets) {
  int32_t min = j_min;
  int32_t max = j_max;
  size_t num_buckets = static_cast<size_t>(j_num_buckets);
  HistogramBase* histogram = HistogramFromHint(j_histogram_hint);
  if (histogram) {
    CheckHistogramArgs(env, j_histogram_name, min, max, num_buckets, histogram);
    return histogram;
  }

  std::string histogram_name = j_histogram_name;
  histogram =
      LinearHistogram::FactoryGet(histogram_name, min, max, num_buckets,
                                  HistogramBase::kUmaTargetedHistogramFlag);
  return histogram;
}

HistogramBase* SparseHistogram(JNIEnv* env,
                               const std::string& histogram_name,
                               int64_t j_histogram_hint) {
  HistogramBase* histogram = HistogramFromHint(j_histogram_hint);
  if (histogram) {
    return histogram;
  }

  histogram = SparseHistogram::FactoryGet(
      histogram_name, HistogramBase::kUmaTargetedHistogramFlag);
  return histogram;
}

struct ActionCallbackWrapper {
  base::ActionCallback action_callback;
};

static void OnActionRecorded(const JavaRef<jobject>& callback,
                             const std::string& action,
                             TimeTicks action_time) {
  RunStringCallbackAndroid(callback, action);
}

}  // namespace

static int64_t JNI_NativeUmaRecorder_RecordBooleanHistogram(
    JNIEnv* env,
    const std::string& j_histogram_name,
    int64_t j_histogram_hint,
    bool j_sample) {
  bool sample = j_sample;
  HistogramBase* histogram =
      BooleanHistogram(env, j_histogram_name, j_histogram_hint);
  histogram->AddBoolean(sample);
  return reinterpret_cast<int64_t>(histogram);
}

static int64_t JNI_NativeUmaRecorder_RecordExponentialHistogram(
    JNIEnv* env,
    const std::string& j_histogram_name,
    int64_t j_histogram_hint,
    int32_t j_sample,
    int32_t j_min,
    int32_t j_max,
    int32_t j_num_buckets) {
  int sample = static_cast<int>(j_sample);
  HistogramBase* histogram = ExponentialHistogram(
      env, j_histogram_name, j_histogram_hint, j_min, j_max, j_num_buckets);
  histogram->Add(sample);
  return reinterpret_cast<int64_t>(histogram);
}

static int64_t JNI_NativeUmaRecorder_RecordLinearHistogram(
    JNIEnv* env,
    const std::string& j_histogram_name,
    int64_t j_histogram_hint,
    int32_t j_sample,
    int32_t j_min,
    int32_t j_max,
    int32_t j_num_buckets) {
  int sample = static_cast<int>(j_sample);
  HistogramBase* histogram = LinearHistogram(
      env, j_histogram_name, j_histogram_hint, j_min, j_max, j_num_buckets);
  histogram->Add(sample);
  return reinterpret_cast<int64_t>(histogram);
}

static int64_t JNI_NativeUmaRecorder_RecordSparseHistogram(
    JNIEnv* env,
    const std::string& j_histogram_name,
    int64_t j_histogram_hint,
    int32_t j_sample) {
  int sample = static_cast<int>(j_sample);
  HistogramBase* histogram =
      SparseHistogram(env, j_histogram_name, j_histogram_hint);
  histogram->Add(sample);
  return reinterpret_cast<int64_t>(histogram);
}

static void JNI_NativeUmaRecorder_RecordUserAction(
    JNIEnv* env,
    const std::string& user_action_name,
    int64_t j_millis_since_event) {
  // Time values coming from Java need to be synchronized with TimeTick clock.
  RecordComputedActionSince(user_action_name,
                            Milliseconds(j_millis_since_event));
}

// This backs a Java test util for testing histograms -
// MetricsUtils.HistogramDelta. It should live in a test-specific file, but we
// currently can't have test-specific native code packaged in test-specific Java
// targets - see http://crbug.com/415945.
static int32_t JNI_NativeUmaRecorder_GetHistogramValueCountForTesting(
    JNIEnv* env,
    const std::string& name,
    int32_t sample,
    int64_t snapshot_ptr) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram (yet?).
    return 0;
  }

  int actual_count = histogram->SnapshotSamples()->GetCount(sample);
  if (snapshot_ptr) {
    auto* snapshot = reinterpret_cast<HistogramsSnapshot*>(snapshot_ptr);
    auto snapshot_data = snapshot->find(name);
    if (snapshot_data != snapshot->end()) {
      actual_count -= snapshot_data->second->GetCount(sample);
    }
  }

  return actual_count;
}

static int32_t JNI_NativeUmaRecorder_GetHistogramTotalCountForTesting(
    JNIEnv* env,
    const std::string& name,
    int64_t snapshot_ptr) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  if (histogram == nullptr) {
    // No samples have been recorded for this histogram.
    return 0;
  }

  int actual_count = histogram->SnapshotSamples()->TotalCount();
  if (snapshot_ptr) {
    auto* snapshot = reinterpret_cast<HistogramsSnapshot*>(snapshot_ptr);
    auto snapshot_data = snapshot->find(name);
    if (snapshot_data != snapshot->end()) {
      actual_count -= snapshot_data->second->TotalCount();
    }
  }
  return actual_count;
}

// Returns an array with 3 entries for each bucket, representing (min, max,
// count).
static ScopedJavaLocalRef<jlongArray>
JNI_NativeUmaRecorder_GetHistogramSamplesForTesting(JNIEnv* env,
                                                    const std::string& name) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(name);
  std::vector<int64_t> buckets;

  if (histogram == nullptr) {
    // No samples have been recorded for this histogram.
    return base::android::ToJavaLongArray(env, buckets);
  }

  std::unique_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  for (auto sampleCountIterator = samples->Iterator();
       !sampleCountIterator->Done(); sampleCountIterator->Next()) {
    HistogramBase::Sample32 min;
    int64_t max;
    HistogramBase::Count32 count;
    sampleCountIterator->Get(&min, &max, &count);
    buckets.push_back(min);
    buckets.push_back(max);
    buckets.push_back(count);
  }

  return base::android::ToJavaLongArray(env, buckets);
}

static int64_t JNI_NativeUmaRecorder_CreateHistogramSnapshotForTesting(
    JNIEnv* env) {
  HistogramsSnapshot* snapshot = new HistogramsSnapshot();
  for (const auto* const histogram : StatisticsRecorder::GetHistograms()) {
    InsertOrAssign(*snapshot, histogram->histogram_name(),
                   histogram->SnapshotSamples());
  }
  return reinterpret_cast<intptr_t>(snapshot);
}

static void JNI_NativeUmaRecorder_DestroyHistogramSnapshotForTesting(
    JNIEnv* env,
    int64_t snapshot_ptr) {
  delete reinterpret_cast<HistogramsSnapshot*>(snapshot_ptr);
}

static int64_t JNI_NativeUmaRecorder_AddActionCallbackForTesting(
    JNIEnv* env,
    const JavaRef<jobject>& callback) {
  // Create a wrapper for the ActionCallback, so it can life on the heap until
  // RemoveActionCallbackForTesting() is called.
  auto* wrapper = new ActionCallbackWrapper{base::BindRepeating(
      &OnActionRecorded, ScopedJavaGlobalRef<jobject>(env, callback))};
  base::AddActionCallback(wrapper->action_callback);
  return reinterpret_cast<intptr_t>(wrapper);
}

static void JNI_NativeUmaRecorder_RemoveActionCallbackForTesting(
    JNIEnv* env,
    int64_t callback_id) {
  DCHECK(callback_id);
  auto* wrapper = reinterpret_cast<ActionCallbackWrapper*>(callback_id);
  base::RemoveActionCallback(wrapper->action_callback);
  delete wrapper;
}

}  // namespace android
}  // namespace base

DEFINE_JNI(NativeUmaRecorder)
