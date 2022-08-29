// Copyright 2022 The Chromium Authors. All rights reserved.
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
constexpr auto kGracePeriod = base::Seconds(2);

constexpr double kMaxAverageFailureScore = 0.1;  // Two failures out of 20.

// Samples for different playback/CDM/crash events.
constexpr int kSignificantPlayback = 0;  // This must always be zero.
constexpr int kPlaybackOrCdmError = 1;
constexpr int kCrash = 1;

// We store a list of timestamps in "Local State" (see about://local-state)
// under the key "media.hardware_secure_decryption.disabled_times". This is
// the maximum number of disabled times we store. We may not use all of them
// for now but may need them in the future when we refine the algorithm.
constexpr int kMaxNumberOfDisabledTimesInPref = 3;

// Number of days to keep disabling hardware secure decryption after it's
// disabled previously because of errors.
constexpr int kDaysDisablingExtended = 7;

// Gets the list of disabled times from "Local State".
std::vector<base::Time> GetDisabledTimesPref() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  std::vector<base::Time> times;
  for (const base::Value& time_value :
       service->GetList(prefs::kHardwareSecureDecryptionDisabledTimes)
           ->GetListDeprecated()) {
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

  base::ListValue time_list;
  for (auto time : times)
    time_list.Append(base::TimeToValue(time));

  service->Set(prefs::kHardwareSecureDecryptionDisabledTimes, time_list);
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
// TODO(crbug.com/1296219): Refine this disabling algorithm.
bool MediaFoundationServiceMonitor::IsHardwareSecureDecryptionDisabledByPref() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::vector<base::Time> disabled_times = GetDisabledTimesPref();
  base::Time current_time = base::Time::Now();
  for (const auto& disabled_time : disabled_times) {
    if (current_time - disabled_time < base::Days(kDaysDisablingExtended))
      return true;
  }
  return false;
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

  bool is_after_power_or_display_change =
      (base::Time::Now() - last_power_or_display_change_time_) < kGracePeriod;
  base::UmaHistogramBoolean("Media.EME.MediaFoundationService.Crash",
                            is_after_power_or_display_change);

  // Not checking `last_power_or_display_change_time_`; crashes are always bad.
  DVLOG(1) << __func__ << ": MediaFoundationService process crashed!";
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

  if (base::Time::Now() - last_power_or_display_change_time_ < kGracePeriod) {
    DVLOG(1) << "Playback or CDM error ignored since it happened right after "
                "a power or display change.";
    base::UmaHistogramSparse(
        "Media.EME.MediaFoundationService.ErrorAfterPowerOrDisplayChange", hr);
    return;
  }

  base::UmaHistogramSparse(
      "Media.EME.MediaFoundationService.ErrorNotAfterPowerOrDisplayChange", hr);
  AddSample(kPlaybackOrCdmError);
}

void MediaFoundationServiceMonitor::OnPowerOrDisplayChange() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  last_power_or_display_change_time_ = base::Time::Now();
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
