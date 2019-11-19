// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tracing/crash_service_uploader.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/tracing_switches.h"
#include "components/variations/active_field_trials.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/crash_upload_list/crash_upload_list_android.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#endif

namespace {

const int kMinDaysUntilNextUpload = 7;

// These values are logged to UMA. Entries should not be renumbered and numeric
// values should never be reused. Please keep in sync with
// "TracingFinalizationDisallowedReason" in
// src/tools/metrics/histograms/enums.xml.
enum class TracingFinalizationDisallowedReason {
  kIncognitoLaunched = 0,
  kProfileNotLoaded = 1,
  kCrashMetricsNotLoaded = 2,
  kLastSessionCrashed = 3,
  kMetricsReportingDisabled = 4,
  kTraceUploadedRecently = 5,
  kMaxValue = kTraceUploadedRecently
};

void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.FinalizationDisallowedReason",
                            reason);
}

}  // namespace

void ChromeTracingDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterInt64Pref(prefs::kBackgroundTracingLastUpload, 0);
}

ChromeTracingDelegate::ChromeTracingDelegate() : incognito_launched_(false) {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if !defined(OS_ANDROID)
  BrowserList::AddObserver(this);
#else
  TabModelList::AddObserver(this);
#endif
}

ChromeTracingDelegate::~ChromeTracingDelegate() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if !defined(OS_ANDROID)
  BrowserList::RemoveObserver(this);
#else
  TabModelList::RemoveObserver(this);
#endif
}

#if defined(OS_ANDROID)
void ChromeTracingDelegate::OnTabModelAdded() {
  for (TabModelList::const_iterator i = TabModelList::begin();
       i != TabModelList::end(); i++) {
    if ((*i)->GetProfile()->IsOffTheRecord())
      incognito_launched_ = true;
  }
}

void ChromeTracingDelegate::OnTabModelRemoved() {}

#else

void ChromeTracingDelegate::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->IsOffTheRecord())
    incognito_launched_ = true;
}
#endif  // defined(OS_ANDROID)

std::unique_ptr<content::TraceUploader> ChromeTracingDelegate::GetTraceUploader(
    scoped_refptr<network::SharedURLLoaderFactory> factory) {
  return std::unique_ptr<content::TraceUploader>(
      new TraceCrashServiceUploader(std::move(factory)));
}

namespace {

enum PermitMissingProfile { PROFILE_REQUIRED, PROFILE_NOT_REQUIRED };

Profile* GetProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  return profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir()));
}

bool ProfileAllowsScenario(const content::BackgroundTracingConfig& config,
                           PermitMissingProfile profile_permission) {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced.
  auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableBackgroundTracing) &&
      command_line->HasSwitch(switches::kTraceUploadURL)) {
    return true;
  }

  // If the profile hasn't loaded or been created yet, we allow the scenario
  // to start up, but not be finalized.
  Profile* profile = GetProfile();
  if (!profile) {
    if (profile_permission == PROFILE_REQUIRED) {
      RecordDisallowedMetric(
          TracingFinalizationDisallowedReason::kProfileNotLoaded);
    }
    return profile_permission != PROFILE_REQUIRED;
  }

// Safeguard, in case background tracing is responsible for a crash on
// startup.
#if !defined(OS_ANDROID)
  if (profile->GetLastSessionExitType() == Profile::EXIT_CRASHED) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kLastSessionCrashed);
    return false;
  }
#else
  // If the metrics haven't loaded, we allow the scenario to start up, but not
  // be finalized.
  if (!CrashUploadListAndroid::BrowserCrashMetricsInitialized()) {
    if (profile_permission == PROFILE_REQUIRED) {
      RecordDisallowedMetric(
          TracingFinalizationDisallowedReason::kCrashMetricsNotLoaded);
    }
    return profile_permission != PROFILE_REQUIRED;
  }

  if (CrashUploadListAndroid::DidBrowserCrashRecently()) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kLastSessionCrashed);
    return false;
  }
#endif

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

#if !defined(OS_CHROMEOS) && defined(OFFICIAL_BUILD)
  if (!local_state->GetBoolean(metrics::prefs::kMetricsReportingEnabled)) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kMetricsReportingDisabled);
    return false;
  }
#endif // !OS_CHROMEOS && OFFICIAL_BUILD

  if (config.tracing_mode() == content::BackgroundTracingConfig::PREEMPTIVE) {
    const base::Time last_upload_time = base::Time::FromInternalValue(
        local_state->GetInt64(prefs::kBackgroundTracingLastUpload));
    if (!last_upload_time.is_null()) {
      base::Time computed_next_allowed_time =
          last_upload_time + base::TimeDelta::FromDays(kMinDaysUntilNextUpload);
      if (computed_next_allowed_time > base::Time::Now()) {
        RecordDisallowedMetric(
            TracingFinalizationDisallowedReason::kTraceUploadedRecently);
        return false;
      }
    }
  }

  return true;
}

}  // namespace

bool ChromeTracingDelegate::IsAllowedToBeginBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  if (!ProfileAllowsScenario(config, PROFILE_NOT_REQUIRED))
    return false;

  if (requires_anonymized_data && chrome::IsIncognitoSessionActive())
    return false;

  return true;
}

bool ChromeTracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  if (requires_anonymized_data &&
      (incognito_launched_ || chrome::IsIncognitoSessionActive())) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kIncognitoLaunched);
    return false;
  }

  if (!ProfileAllowsScenario(config, PROFILE_REQUIRED))
    return false;

  if (config.tracing_mode() == content::BackgroundTracingConfig::PREEMPTIVE) {
    PrefService* local_state = g_browser_process->local_state();
    DCHECK(local_state);
    local_state->SetInt64(prefs::kBackgroundTracingLastUpload,
                          base::Time::Now().ToInternalValue());

    // We make sure we've persisted the value in case finalizing the tracing
    // causes a crash.
    local_state->CommitPendingWrite();
  }

  return true;
}

bool ChromeTracingDelegate::IsProfileLoaded() {
  return GetProfile() != nullptr;
}

std::unique_ptr<base::DictionaryValue>
ChromeTracingDelegate::GenerateMetadataDict() {
  auto metadata_dict = std::make_unique<base::DictionaryValue>();
  std::vector<std::string> variations;
  variations::GetFieldTrialActiveGroupIdsAsStrings(base::StringPiece(),
                                                   &variations);

  std::unique_ptr<base::ListValue> variations_list(new base::ListValue());
  for (const auto& it : variations)
    variations_list->AppendString(it);

  metadata_dict->Set("field-trials", std::move(variations_list));
  metadata_dict->SetString("revision", version_info::GetLastChange());
  return metadata_dict;
}
