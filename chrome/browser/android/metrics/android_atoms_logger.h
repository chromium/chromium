// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_METRICS_ANDROID_ATOMS_LOGGER_H_
#define CHROME_BROWSER_ANDROID_METRICS_ANDROID_ATOMS_LOGGER_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "chrome/browser/android/metrics/westworld_histogram_allowlist.h"
#include "components/prefs/pref_change_registrar.h"

namespace chrome::android::westworld {

// AndroidAtomsLogger is a singleton that listens to UMA histograms and logs
// them to Westworld.
class AndroidAtomsLogger {
 public:
  static void Initialize();
  static bool IsDesktop();

  AndroidAtomsLogger(const AndroidAtomsLogger&) = delete;
  AndroidAtomsLogger& operator=(const AndroidAtomsLogger&) = delete;

 protected:
  // Constructor used for dependency injection in tests.
  //
  // - `allowlist`: The list of histograms to observe.
  explicit AndroidAtomsLogger(base::span<const HistogramInfo> allowlist);

  virtual ~AndroidAtomsLogger();

  // Logs the atom. Default implementation calls JNI to log to Westworld.
  // Overridden in tests to verify atom logging.
  virtual void LogAtom(int atom_id,
                       MetricType type,
                       base::HistogramBase::Sample32 sample);

 private:
  friend class base::NoDestructor<AndroidAtomsLogger>;
  FRIEND_TEST_ALL_PREFIXES(AndroidAtomsLoggerTest,
                           FeatureDisabled_DoesNotInitialize);
  FRIEND_TEST_ALL_PREFIXES(AndroidAtomsLoggerTest,
                           FeatureEnabled_InitializesOnDesktop);
  FRIEND_TEST_ALL_PREFIXES(AndroidAtomsLoggerTest,
                           FeatureEnabled_DoesNotInitializeOnNonDesktop);
  FRIEND_TEST_ALL_PREFIXES(AndroidAtomsLoggerTest,
                           LogAtomGuardedByMetricsConsentOnDesktop);

  // Production constructor.
  AndroidAtomsLogger();

  void InitializePrefRegistrar();
  void OnMetricsConsentChanged();
  void OnHistogramSample(int atom_id,
                         MetricType type,
                         std::string_view name,
                         uint64_t name_hash,
                         base::HistogramBase::Sample32 sample);

  bool metrics_reporting_enabled_ = true;
  PrefChangeRegistrar pref_change_registrar_;
  base::CallbackListSubscription consent_change_subscription_;
  std::vector<
      std::unique_ptr<base::StatisticsRecorder::ScopedHistogramSampleObserver>>
      observers_;
};

}  // namespace chrome::android::westworld

#endif  // CHROME_BROWSER_ANDROID_METRICS_ANDROID_ATOMS_LOGGER_H_
