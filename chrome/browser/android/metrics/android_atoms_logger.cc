// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/metrics/android_atoms_logger.h"

#include "base/android/device_info.h"
#include "base/android/jni_android.h"
#include "base/functional/bind.h"
#include "base/functional/function_ref.h"
#include "base/logging.h"
#include "chrome/browser/android/metrics/jni_headers/ChromeStatsLogHelper_jni.h"
#include "chrome/browser/android/metrics/westworld_histogram_allowlist.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"

namespace chrome::android::westworld {

// static
void AndroidAtomsLogger::Initialize() {
  static base::NoDestructor<AndroidAtomsLogger> instance;
  instance.get();
}

AndroidAtomsLogger::AndroidAtomsLogger()
    : AndroidAtomsLogger(kWestworldHistogramAllowlist) {}

AndroidAtomsLogger::AndroidAtomsLogger(
    base::span<const HistogramInfo> allowlist) {
  if (!base::FeatureList::IsEnabled(kAndroidAtomsLogging)) {
    return;
  }

  if (!base::android::device_info::is_desktop()) {
    // The feature to log UMA histograms as Atoms is only intended to be enabled
    // for Android Desktop for now.
    return;
  }

  for (const HistogramInfo& histogram_info : allowlist) {
    observers_.push_back(
        std::make_unique<
            base::StatisticsRecorder::ScopedHistogramSampleObserver>(
            histogram_info.histogram_name,
            base::BindRepeating(
                &AndroidAtomsLogger::OnHistogramSample, base::Unretained(this),
                histogram_info.ww_atom_id, histogram_info.type)));
  }
}

AndroidAtomsLogger::~AndroidAtomsLogger() = default;

void AndroidAtomsLogger::LogAtom(int atom_id,
                                 MetricType type,
                                 base::HistogramBase::Sample32 sample) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (type == MetricType::kInt) {
    ::metrics::Java_ChromeStatsLogHelper_logHistogramAsIntTypeAtom(env, atom_id,
                                                                   sample);
  } else if (type == MetricType::kBoolean) {
    ::metrics::Java_ChromeStatsLogHelper_logHistogramAsBooleanTypeAtom(
        env, atom_id, sample != 0);
  }
}

void AndroidAtomsLogger::OnHistogramSample(
    int atom_id,
    MetricType type,
    std::string_view name,
    uint64_t name_hash,
    base::HistogramBase::Sample32 sample) {
  LogAtom(atom_id, type, sample);
}

}  // namespace chrome::android::westworld
