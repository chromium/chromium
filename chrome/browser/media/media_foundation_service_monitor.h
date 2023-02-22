// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MEDIA_FOUNDATION_SERVICE_MONITOR_H_
#define CHROME_BROWSER_MEDIA_MEDIA_FOUNDATION_SERVICE_MONITOR_H_

#include "base/power_monitor/moving_average.h"
#include "base/power_monitor/power_observer.h"
#include "base/time/time.h"
#include "content/public/browser/service_process_host.h"
#include "ui/display/display_observer.h"

class PrefRegistrySimple;

class MediaFoundationServiceMonitor final
    : public content::ServiceProcessHost::Observer,
      public base::PowerSuspendObserver,
      public display::DisplayObserver {
 public:
  // Register the pref used in this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the earliest time when hardware secure decryption should be
  // re-enabled after previous `disabled_times`.
  static base::Time GetEarliestEnableTime(
      std::vector<base::Time> disabled_times);

  // Same as above, but uses base::Time::Now() as the `current_time`, and get
  // `disabled_times` from "Local State".
  static bool IsHardwareSecureDecryptionDisabledByPref();

  // Returns the MediaFoundationServiceMonitor singleton.
  static MediaFoundationServiceMonitor* GetInstance();

  MediaFoundationServiceMonitor(const MediaFoundationServiceMonitor&) = delete;
  MediaFoundationServiceMonitor& operator=(
      const MediaFoundationServiceMonitor&) = delete;

  // ServiceProcessHost::Observer implementation.
  void OnServiceProcessCrashed(const content::ServiceProcessInfo& info) final;

  // base::PowerSuspendObserver implementation.
  void OnSuspend() final;
  void OnResume() final;

  // display::DisplayObserver implementation.
  void OnDisplayAdded(const display::Display& new_display) final;
  void OnDidRemoveDisplays() final;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) final;

  // Called when a significant playback or error happened when using
  // MediaFoundationCdm.
  void OnSignificantPlayback();
  void OnPlaybackOrCdmError(HRESULT hr);
  void OnUnexpectedHardwareContextReset();

  // Whether there was any recent power or display change.
  bool HasRecentPowerOrDisplayChange() const;

 private:
  // Make constructor/destructor private since this is a singleton.
  MediaFoundationServiceMonitor();
  ~MediaFoundationServiceMonitor() final;

  void OnPowerOrDisplayChange();

  // Adds a sample with failure score. Zero means success. The higher the value
  // the more severe the error is. See the .cc file for details.
  void AddSample(int failure_score);

  // Last time when a power or display event happened. This is used to ignore
  // playback or CDM errors caused by those events. For example, playback
  // failure caused by user plugging in a non-HDCP monitor, but the content
  // requires HDCP enforcement.
  base::TimeTicks last_power_or_display_change_time_;

  // Keep track the last fixed length reported samples (scores). The average
  // score is used to decide whether to disable hardware secure decryption.
  base::MovingAverage samples_;
};

#endif  // CHROME_BROWSER_MEDIA_MEDIA_FOUNDATION_SERVICE_MONITOR_H_
