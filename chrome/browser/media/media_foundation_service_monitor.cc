// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/media_foundation_service_monitor.h"

#include <algorithm>
#include <memory>
#include <set>
#include <vector>

#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/power_monitor/power_monitor.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/media/cdm_pref_service_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/cdm_registry.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/display/screen.h"
#include "url/gurl.h"
#include "url/origin.h"

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
std::vector<base::Time> GetDisabledTimesGlobal() {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  std::vector<base::Time> times;
  for (const base::Value& time_value :
       service->GetList(prefs::kGlobalHardwareSecureDecryptionDisabledTimes)) {
    auto time = base::ValueToTime(time_value);
    if (time.has_value())
      times.push_back(time.value());
  }

  return times;
}

// Sets the list of disabled times in "Local State".
void SetDisabledTimesGlobal(std::vector<base::Time> times) {
  PrefService* service = g_browser_process->local_state();
  DCHECK(service);

  base::Value::List time_list;
  for (auto time : times)
    time_list.Append(base::TimeToValue(time));

  service->SetList(prefs::kGlobalHardwareSecureDecryptionDisabledTimes,
                   std::move(time_list));
}

// Gets a dictionary key from site. This is used to map origins to sites. E.g.
// subdomain.domain.com becomes domain.com.
std::string GetDictKeyFromSite(const GURL& site) {
  return net::registry_controlled_domains::GetDomainAndRegistry(
      site, net::registry_controlled_domains::PrivateRegistryFilter::
                EXCLUDE_PRIVATE_REGISTRIES);
}

// Returns all origins that map to a site from a dictionary.
std::vector<std::string> GetOriginsForSite(const base::Value::Dict& origin_dict,
                                           const GURL& site) {
  std::vector<std::string> result;
  for (auto [origin_str, value] : origin_dict) {
    auto origin_url = GURL(origin_str);
    if (GetDictKeyFromSite(origin_url) == GetDictKeyFromSite(site)) {
      result.push_back(origin_str);
    }
  }
  return result;
}

// Gets the list of disabled times from "Pref Service".
// If there are no disabled times for site, Use "Local State" to check
// disabled times. CDM pref service helper will create origin id mapping and
// will initialize "Pref Service" entry.
std::vector<base::Time> GetDisabledTimesPerSite(const GURL& site) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  CHECK(profile);
  PrefService* user_prefs = profile->GetPrefs();
  CHECK(user_prefs);

  auto& origin_dict = user_prefs->GetDict(prefs::kMediaCdmOriginData);
  auto origins = GetOriginsForSite(origin_dict, site);

  // On first visit, use "Local State" disabled times.
  // `origins` will get populated with local state if playback is successful
  // eventually when we initialize te pref entry.
  if (origins.empty()) {
    return GetDisabledTimesGlobal();
  }

  std::set<base::Time> times;
  for (auto origin : origins) {
    const base::Value::List* list = origin_dict.FindDict(origin)->FindList(
        prefs::kHardwareSecureDecryptionDisabledTimes);
    if (list) {
      for (const base::Value& time_value : *list) {
        auto time = base::ValueToTime(time_value);
        if (time.has_value()) {
          times.insert(time.value());
        }
      }
    }
  }

  return {times.begin(), times.end()};
}

// Sets the list of disabled times in "Pref Service" for a site.
void SetDisabledTimesPerSite(const GURL& site, std::vector<base::Time> times) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  CHECK(profile);
  PrefService* user_prefs = profile->GetPrefs();
  CHECK(user_prefs);

  ScopedDictPrefUpdate update(user_prefs, prefs::kMediaCdmOriginData);

  // Find all origins that maps to site.
  auto origins = GetOriginsForSite(update.Get(), site);

  for (auto origin : origins) {
    base::Value::Dict* origin_dict = update->FindDict(origin);
    if (!origin_dict) {
      continue;
    }

    auto* list = update->FindDict(origin)->EnsureList(
        prefs::kHardwareSecureDecryptionDisabledTimes);
    list->clear();
    for (auto time : times) {
      list->Append(base::TimeToValue(time));
    }
  }
}

// Sorts the times in descending order so the resize will drop the oldest time.
std::vector<base::Time> CappedTimes(std::vector<base::Time> times) {
  std::sort(times.begin(), times.end(), std::greater<>());
  if (times.size() > kMaxNumberOfDisabledTimesInPref) {
    times.resize(kMaxNumberOfDisabledTimesInPref);
  }
  return times;
}

// Adds a new time to the list of disabled times "Local State".
void AddDisabledTimeGlobal(base::Time time) {
  std::vector<base::Time> disabled_times = GetDisabledTimesGlobal();
  disabled_times.push_back(time);
  SetDisabledTimesGlobal(CappedTimes(disabled_times));
}

// Adds a new time to the list of disabled times in "Pref Service".
void AddDisabledTimePerSite(const GURL& site, base::Time time) {
  std::vector<base::Time> disabled_times = GetDisabledTimesPerSite(site);
  disabled_times.push_back(time);
  SetDisabledTimesPerSite(site, CappedTimes(disabled_times));
}

}  // namespace

// static
void MediaFoundationServiceMonitor::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(
      prefs::kGlobalHardwareSecureDecryptionDisabledTimes);
}

// static
base::Time MediaFoundationServiceMonitor::GetEarliestEnableTime(
    std::vector<base::Time> disabled_times) {
  // No disabled time. No need to disable the feature.
  if (disabled_times.empty()) {
    return base::Time::Min();
  }

  // The disabled times should be sorted already. But since they are from the
  // local state or user profile, sort it again just in case.
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
             << ": disabling_duration=" << disabling_duration.InDays();
  }

  return last_disabled_time + disabling_duration;
}

// static
bool MediaFoundationServiceMonitor::IsHardwareSecureDecryptionDisabledByPref() {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto earliest_enable_time = GetEarliestEnableTime(GetDisabledTimesGlobal());
  DVLOG(1) << __func__ << ": earliest_enable_time=" << earliest_enable_time;

  return base::Time::Now() < earliest_enable_time;
}

// static
bool MediaFoundationServiceMonitor::IsHardwareSecureDecryptionAllowedForSite(
    const GURL& site) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static bool allow_per_site_uma_logged = false;

  auto earliest_enable_time =
      GetEarliestEnableTime(GetDisabledTimesPerSite(site));
  DVLOG(1) << __func__ << ": site='" << site
           << "' earliest_enable_time=" << earliest_enable_time;

  bool result = base::Time::Now() > earliest_enable_time;

  // Note: This UMA gets reported once per browser session. It is possible that
  // multiple sites report different result. However, only the first visited
  // site will record UMA until the browser session restarts.
  if (!allow_per_site_uma_logged) {
    base::UmaHistogramBoolean(
        "Media.EME.Widevine.HardwareSecure.AllowedForSite", result);
    allow_per_site_uma_logged = true;
  }

  return result;
}

// static
MediaFoundationServiceMonitor* MediaFoundationServiceMonitor::GetInstance() {
  static auto* monitor = new MediaFoundationServiceMonitor();
  return monitor;
}

void MediaFoundationServiceMonitor::Initialize() {
  DVLOG(1) << __func__;
  // Initialize samples with success cases so the average score won't be
  // dominated by one error. No need to report UMA here.
  for (int i = 0; i < kMaxNumberOfSamples; ++i)
    AddGlobalSample(kSignificantPlayback, base::Time::Now());

  content::ServiceProcessHost::AddObserver(this);
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

void MediaFoundationServiceMonitor::ResetForTesting() {
  DVLOG(1) << __func__;
  global_samples_.Reset();
  samples_.clear();
  last_power_or_display_change_time_ = base::TimeTicks::Min();
}

MediaFoundationServiceMonitor::MediaFoundationServiceMonitor()
    : global_samples_(kMaxNumberOfSamples) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  Initialize();
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
  AddSample(info.site().value(), kCrash, base::Time::Now());
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
void MediaFoundationServiceMonitor::OnDisplaysRemoved(
    const display::Displays& /*removed_displays*/) {
  OnPowerOrDisplayChange();
}
void MediaFoundationServiceMonitor::OnDisplayMetricsChanged(
    const display::Display& /*display*/,
    uint32_t /*changed_metrics*/) {
  OnPowerOrDisplayChange();
}

void MediaFoundationServiceMonitor::OnSignificantPlayback(const GURL& site) {
  DVLOG(1) << __func__ << ": site=" << site;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  AddSample(site, kSignificantPlayback, base::Time::Now());
}

void MediaFoundationServiceMonitor::OnPlaybackOrCdmError(const GURL& site,
                                                         HRESULT hr) {
  DVLOG(1) << __func__ << ": site=" << site;
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
  AddSample(site, kPlaybackOrCdmError, base::Time::Now());
}

void MediaFoundationServiceMonitor::OnUnexpectedHardwareContextReset(
    const GURL& site) {
  DVLOG(1) << __func__;
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (media::kHardwareSecureDecryptionFallbackOnHardwareContextReset.Get()) {
    AddSample(site, kUnexpectedHardwareContextReset, base::Time::Now());
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

void MediaFoundationServiceMonitor::AddSample(const GURL& site,
                                              int failure_score,
                                              base::Time time) {
  AddGlobalSample(failure_score, time);

  // Ensure map of samples for `site` is constructed.
  auto [iterator, added] = samples_.try_emplace(site, kMaxNumberOfSamples);
  auto& moving_average = iterator->second;
  if (added) {
    // Initialize samples with success cases so the average score won't be
    // dominated by one error. No need to report UMA here.
    for (int i = 0; i < kMaxNumberOfSamples; ++i) {
      moving_average.AddSample(kSignificantPlayback);
    }
  }
  moving_average.AddSample(failure_score);

  // When the max average failure score is reached, update the Pref with
  // the new disabled time.
  if (moving_average.Mean<double>() >= kMaxAverageFailureScore) {
    AddDisabledTimePerSite(site, time);
  }
}

void MediaFoundationServiceMonitor::AddGlobalSample(int failure_score,
                                                    base::Time time) {
  global_samples_.AddSample(failure_score);

  // When the max average failure score is reached, always update the local
  // state with the new disabled time, but only actually disable hardware secure
  // decryption globally when fallback is allowed (by the feature).
  if (global_samples_.Mean<double>() >= kMaxAverageFailureScore) {
    AddDisabledTimeGlobal(time);
    if (base::FeatureList::IsEnabled(
            media::kHardwareSecureDecryptionFallback) &&
        !media::kHardwareSecureDecryptionFallbackPerSite.Get()) {
      content::CdmRegistry::GetInstance()->SetHardwareSecureCdmStatus(
          content::CdmInfo::Status::kDisabledOnError);
    }
  }
}
