// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_PERF_METRIC_PROVIDER_H_
#define CHROME_BROWSER_METRICS_PERF_METRIC_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/power_monitor/power_observer.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/metrics/perf/metric_collector.h"

namespace base {
class TimeDelta;
}  // namespace base

class ProfileManager;
class Profile;

namespace metrics {

class SampledProfile;

// MetricProvider manages a metric collector implementation and provides a
// common interface for metric collectors with custom trigger definitions.
// The provider runs its collector on a dedicated sequence. Trigger events
// received on the UI or other threads, are passed to the managed collector
// on its dedicated sequence, via PostTask messages.
class MetricProvider {
 public:
  MetricProvider(std::unique_ptr<internal::MetricCollector> collector,
                 ProfileManager* profile_manager);

  MetricProvider(const MetricProvider&) = delete;
  MetricProvider& operator=(const MetricProvider&) = delete;

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

  // Enables the collector to save profiles to the local cache.
  void EnableRecording();

  // Disables the collector to save profiles to the local cache.
  void DisableRecording();

  void OnJankStarted();
  void OnJankStopped();

  // Updates the known device thermal state.
  void SetThermalState(
      base::PowerThermalObserver::DeviceThermalState new_state) {
    thermal_state_ = new_state;
  }
  // Updates the known cpu speed limit.
  void SetSpeedLimit(int new_limit) { cpu_speed_limit_percent_ = new_limit; }

 protected:
  // Enumeration representing the various outcomes of saving the collected
  // profile to local cache. These values are persisted to logs. Entries should
  // not be renumbererd and numeric values should never be reused.
  enum class RecordAttemptStatus {
    kRecordingDisabled = 0,
    kProfileManagerUnset = 1,
    kNoLoadedProfile = 2,
    kAppSyncDisabled = 3,
    kAppSyncEnabled = 4,
    kSyncServiceUnavailable = 5,
    kChromeSyncFeatureDisabled = 6,
    // Deprecated: kChromeAppSyncDisabled = 7,
    // Deprecated: kOSSyncFeatureDisabled = 8,
    kOSAppSyncDisabled = 9,
    kMaxValue = kOSAppSyncDisabled,
  };

  // For testing.
  void set_cache_updated_callback(base::RepeatingClosure callback) {
    cache_updated_callback_ = std::move(callback);
  }

 private:
  // Callback invoked by the collector on every successful profile capture. It
  // may be invoked on any sequence.
  static void OnProfileDone(base::WeakPtr<MetricProvider> provider,
                            std::unique_ptr<SampledProfile> sampled_profile);

  // Check the state of App Sync for the given user profile.
  RecordAttemptStatus AppSyncStateForUserProfile(Profile* profile);

  // Check the state of App Sync in the current session.
  RecordAttemptStatus GetAppSyncState();

  // Saves a profile to the local cache.
  void AddProfileToCache(std::unique_ptr<SampledProfile> sampled_profile);

  // Indicates if collected profiles can be saved to the local cache.
  bool recording_enabled_ = true;

  // Vector of SampledProfile protobufs containing perf profiles.
  std::vector<SampledProfile> cached_profile_data_;

  // Name of the histogram that counts the number of uploaded reports.
  const std::string upload_uma_histogram_;

  // Name of the histogram that tracks the various outcomes of saving the
  // collected profile to local cache.
  const std::string record_uma_histogram_;

  // The last known device thermal state.
  base::PowerThermalObserver::DeviceThermalState thermal_state_ =
      base::PowerThermalObserver::DeviceThermalState::kUnknown;
  // The last known cpu speed limit.
  int cpu_speed_limit_percent_ = base::PowerThermalObserver::kSpeedLimitMax;

  // Use a dedicated sequence for the collector. Thread safe. Initialized at
  // construction time, then immutable.
  const scoped_refptr<base::SequencedTaskRunner> collector_task_runner_;

  // The metric collector implementation. It is destroyed on the collector
  // sequence after all non-delayed tasks posted by the provider to the sequence
  // have executed.
  std::unique_ptr<internal::MetricCollector> metric_collector_;

  // The profile manager that manages user profiles with their sync settings, we
  // do not own this object and only hold a reference to it.
  raw_ptr<ProfileManager> profile_manager_;

  // Called when |cached_profile_data_| is populated.
  base::RepeatingClosure cache_updated_callback_;

  base::WeakPtrFactory<MetricProvider> weak_factory_;
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_PERF_METRIC_PROVIDER_H_
