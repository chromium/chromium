// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_METRIC_PROVIDER_H_
#define CHROME_BROWSER_METRICS_PERF_METRIC_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/metrics/perf/metric_collector.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace metrics {

class SampledProfile;

// MetricProvider manages a metric collector implementation and provides a
// common interface for metric collectors with custom trigger definitions.
// The provider runs its collector on a dedicated sequence. Trigger events
// received on the UI or other threads, are passed to the managed collector
// on its dedicated sequence, via PostTask messages.
class MetricProvider {
 public:
  explicit MetricProvider(std::unique_ptr<internal::MetricCollector> collector);

  virtual ~MetricProvider();

  void Init();

  // Appends collected perf data protobufs to |sampled_profiles|. Clears all the
  // stored profile data. Returns true if it wrote to |sampled_profiles|.
  bool GetSampledProfiles(std::vector<SampledProfile>* sampled_profiles);

  // Called on a user log in event.
  void OnUserLoggedIn();

  // Turns off profile collection. Called also on a user logout event.
  void Deactivate();

  // Called when a suspend finishes. This is a successful suspend followed by
  // a resume.
  void SuspendDone(base::TimeDelta sleep_duration);

  // Called when a session restore has finished.
  void OnSessionRestoreDone(int num_tabs_restored);

  void OnJankStarted();
  void OnJankStopped();

 protected:
  // For testing.
  void set_cache_updated_callback(base::RepeatingClosure callback) {
    cache_updated_callback_ = std::move(callback);
  }

 private:
  // Callback invoked by the collector on every successful profile capture. It
  // may be invoked on any sequence.
  static void OnProfileDone(base::WeakPtr<MetricProvider> provider,
                            std::unique_ptr<SampledProfile> sampled_profile);

  // Saves a profile to the local cache.
  void AddProfileToCache(std::unique_ptr<SampledProfile> sampled_profile);

  // Vector of SampledProfile protobufs containing perf profiles.
  std::vector<SampledProfile> cached_profile_data_;

  // Name of the histogram that counts the number of uploaded reports.
  const std::string upload_uma_histogram_;

  // Use a dedicated sequence for the collector. Thread safe. Initialized at
  // construction time, then immutable.
  const scoped_refptr<base::SequencedTaskRunner> collector_task_runner_;

  // The metric collector implementation. It is destroyed on the collector
  // sequence after all non-delayed tasks posted by the provider to the sequence
  // have executed.
  std::unique_ptr<internal::MetricCollector> metric_collector_;

  // Called when |cached_profile_data_| is populated.
  base::RepeatingClosure cache_updated_callback_;

  base::WeakPtrFactory<MetricProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MetricProvider);
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_METRIC_PROVIDER_H_
