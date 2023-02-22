// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_foundation_service_monitor.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cdm_registry.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "ui/display/screen.h"

namespace {

// Maximum number of recent samples to consider in the disabling logic.
constexpr int kMaxNumberOfSamples = 20;

// The grace period to ignore errors after a power/display state change.
constexpr auto kGracePeriod = base::Seconds(5);

constexpr double kMaxAverageFailureScore = 0.1;  // Two failures out of 20.

// Samples for different playback/CDM/crash events.
constexpr int kSignificantPlayback = 0;  // This must always be zero.
constexpr int kPlaybackOrCdmError = 1;
constexpr int kCrash = 1;
constexpr int kUnexpectedHardwareContextReset = 1;

// We store a list of timestamps in "Local State" (see about://local-state)
// under the key "media.hardware_secure_decryption.disabled_times". This is
// the maximum number of disabled times we store. We may not use all of them
// for now but may need them in the future when we refine the algorithm.
constexpr int kMaxNumberOfDisabledTimesInPref = 3;

// Gets the list of disabled times from "Local State".
std::vector<base::Time> GetDisabledTimesPref() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  std::vector<base::Time> times;
  for (const base::Value& time_value :
       service->GetList(prefs::kHardwareSecureDecryptionDisabledTimes)) {
    auto time = base::ValueToTime(time_value);
    if (time.has_value())
      times.push_back(time.value());
  }

  return times;
}

// Sets the list of disabled times in "Local State".
void SetDisabledTimesPref(std::vector<base::Time> times) {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  base::Value::List time_list;
  for (auto time : times)
    time_list.Append(base::TimeToValue(time));

  service->SetList(prefs::kHardwareSecureDecryptionDisabledTimes,
                   std::move(time_list));
}

// Adds a new time to the list of disabled times in "Local State".
void AddDisabledTimeToPref(base::Time time) {
  std::vector<base::Time> disabled_times = GetDisabledTimesPref();
  disabled_times.push_back(time);
  // Sort the times in descending order so the resize will drop the oldest time.
  std::sort(disabled_times.begin(), disabled_times.end(), std::greater<>());
  if (disabled_times.size() > kMaxNumberOfDisabledTimesInPref)
    disabled_times.resize(kMaxNumberOfDisabledTimesInPref);
  SetDisabledTimesPref(disabled_times);
}

}  // namespace

// static
void MediaFoundationServiceMonitor::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kHardwareSecureDecryptionDisabledTimes);
}

// static
base::Time MediaFoundationServiceMonitor::GetEarliestEnableTime(
    std::vector<base::Time> disabled_times) {
  // No disabled time. No need to disable the feature.
  if (disabled_times.empty())
    return base::Time::Min();

  // The disabled times should be sorted already. But since they are from the
  // local state, sort it again just in case.
  std::sort(disabled_times.begin(), disabled_times.end(), std::greater<>());

  base::Time last_disabled_time = disabled_times[0];

  // Get and normalize `min_disabling_duration` and `max_disabling_duration`.
  auto min_disabling_duration = base::Days(
      media::kHardwareSecureDecryptionFallbackMinDisablingDays.Get());
  auto max_disabling_duration = base::Days(
      media::kHardwareSecureDecryptionFallbackMaxDisablingDays.Get());
  min_disabling_duration = std::max(min_disabling_duration, base::Days(1));
  max_disabling_duration =
      std::max(max_disabling_duration, min_disabling_duration);

  // One disabled time will disable the feature for `kDaysDisablingExtended`.
  base::TimeDelta disabling_duration = min_disabling_duration;

  // A previous disabled time will cause longer disabling time since the
  // probability of failure is much higher.
  if (disabled_times.size() > 1) {
    base::Time prev_disabled_time = disabled_times[1];

    // Normally the gap should always be greater than kMinDisablingDuration,
    // but there could be exceptions, e.g. when a user manipulates local state
    // directly, or enabling/disabling the fallback manually.
    // Take a max to normalize it and also avoid divided by zero issue.
    auto gap = std::max(last_disabled_time - prev_disabled_time,
                        min_disabling_duration);

    // This is a heuristic algorithm to determine how long we should keep
    // disabling the feature after the `last_disabled_time`, given it was
    // disabled previously at `prev_disabled_time` as well. The closer they are
    // (i.e. the smaller `gap` is), the chance that errors will happen again
    // becomes larger. Two extreme cases:
    // - `gap` is kMinDisablingDuration, meaning the feature was disabled again
    // right after it's re-enabled (after the previous disabling). In this case,
    // disable it for kMaxDisablingCoefficient * kMinDisablingDuration.
    // - `gap` is infinity, which should be equivalent to the case where
    // `prev_disabled_time` doesn't exist. In this case, disable it for
    // `kMinDisablingDuration`.
    // We construct a reciprocal function to satisfy the above properties.
    disabling_duration =
        ((max_disabling_duration - min_disabling_duration) / gap + 1) *
        min_disabling_duration;
    DVLOG(1) << __func__
             << "disabling_duration =" << disabling_duration.InDays();
  }

  return last_disabled_time + disabling_duration;
}

// static
bool MediaFoundationServiceMonitor::IsHardwareSecureDecryptionDisabledByPref() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto earliest_enable_time = GetEarliestEnableTime(GetDisabledTimesPref());
  DVLOG(1) << __func__ << "earliest_enable_time =" << earliest_enable_time;

  return base::Time::Now() < earliest_enable_time;
}

// static
MediaFoundationServiceMonitor* MediaFoundationServiceMonitor::GetInstance() {
  static auto* monitor = new MediaFoundationServiceMonitor();
  return monitor;
}

MediaFoundationServiceMonitor::MediaFoundationServiceMonitor()
    : samples_(kMaxNumberOfSamples) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Initialize samples with success cases so the average score won't be
  // dominated by one error. No need to report UMA here.
  for (int i = 0; i < kMaxNumberOfSamples; ++i)
    AddSample(kSignificantPlayback);

  content::ServiceProcessHost::AddObserver(this);
  base::PowerMonitor::AddPowerSuspendObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

MediaFoundationServiceMonitor::~MediaFoundationServiceMonitor() = default;

void MediaFoundationServiceMonitor::OnServiceProcessCrashed(
    const content::ServiceProcessInfo& info) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Only interested in MediaFoundationService process.
  if (!info.IsService<media::mojom::MediaFoundationServiceBroker>())
    return;

  base::UmaHistogramBoolean("Media.EME.MediaFoundationService.Crash2",
                            HasRecentPowerOrDisplayChange());

  // Site should always be set when launching MediaFoundationService, but it
  // could be empty, e.g. during capability checking or when
  // `media::kCdmProcessSiteIsolation` is disabled.
  DVLOG(1) << __func__ << ": MediaFoundationService process ("
           << info.site().value() << ") crashed!";

  // Not checking `last_power_or_display_change_time_`; crashes are always bad.
  AddSample(kCrash);
}

void MediaFoundationServiceMonitor::OnSuspend() {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnResume() {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnDisplayAdded(
    const display::Display& /*new_display*/) {
  OnPowerOrDisplayChange();
}
void MediaFoundationServiceMonitor::OnDidRemoveDisplays() {
  OnPowerOrDisplayChange();
}
void MediaFoundationServiceMonitor::OnDisplayMetricsChanged(
    const display::Display& /*display*/,
    uint32_t /*changed_metrics*/) {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnSignificantPlayback() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AddSample(kSignificantPlayback);
}

void MediaFoundationServiceMonitor::OnPlaybackOrCdmError(HRESULT hr) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (HasRecentPowerOrDisplayChange()) {
    DVLOG(1) << "Playback or CDM error ignored since it happened right after "
                "a power or display change.";
    base::UmaHistogramSparse(
        "Media.EME.MediaFoundationService.ErrorAfterPowerOrDisplayChange2", hr);
    return;
  }

  base::UmaHistogramSparse(
      "Media.EME.MediaFoundationService.ErrorNotAfterPowerOrDisplayChange2",
      hr);
  AddSample(kPlaybackOrCdmError);
}

void MediaFoundationServiceMonitor::OnUnexpectedHardwareContextReset() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (media::kHardwareSecureDecryptionFallbackOnHardwareContextReset.Get()) {
    AddSample(kUnexpectedHardwareContextReset);
  }
}

bool MediaFoundationServiceMonitor::HasRecentPowerOrDisplayChange() const {
  return base::TimeTicks::Now() - last_power_or_display_change_time_ <
         kGracePeriod;
}

void MediaFoundationServiceMonitor::OnPowerOrDisplayChange() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  last_power_or_display_change_time_ = base::TimeTicks::Now();
}

void MediaFoundationServiceMonitor::AddSample(int failure_score) {
  samples_.AddSample(failure_score);

  // When the max average failure score is reached, always update the local
  // state with the new disabled time, but only actually disable hardware secure
  // decryption when fallback is allowed (by the feature).
  if (samples_.GetUnroundedAverage() >= kMaxAverageFailureScore) {
    AddDisabledTimeToPref(base::Time::Now());
    if (base::FeatureList::IsEnabled(media::kHardwareSecureDecryptionFallback))
      content::CdmRegistry::GetInstance()->SetHardwareSecureCdmStatus(
          content::CdmInfo::Status::kDisabledOnError);
  }
}
