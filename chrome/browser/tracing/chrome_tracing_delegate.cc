// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tracing/background_tracing_field_trial.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/active_field_trials.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/crosapi_pref_observer.h"
#endif

namespace {

constexpr char kTracingStateKey[] = "state";
constexpr char kUploadTimesKey[] = "upload_times";
constexpr char kScenarioKey[] = "scenario";
constexpr char kUploadTimestampKey[] = "time";

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
  kLastTracingSessionDidNotEnd = 6,
  kMaxValue = kLastTracingSessionDidNotEnd
};

void RecordDisallowedMetric(TracingFinalizationDisallowedReason reason) {
  UMA_HISTOGRAM_ENUMERATION("Tracing.Background.FinalizationDisallowedReason",
                            reason);
}

bool IsBackgroundTracingCommandLine() {
  if (tracing::GetBackgroundTracingSetupMode() ==
      tracing::BackgroundTracingSetupMode::kFromConfigFile) {
    return true;
  }
  return false;
}

// Removes any version numbers from the scenario name.
std::string StripScenarioName(const std::string& scenario_name) {
  std::string stripped_scenario_name;
  base::RemoveChars(scenario_name, "1234567890", &stripped_scenario_name);
  return stripped_scenario_name;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Helper for reading the value of device policy from ash-chrome.
class DevicePolicyObserver {
 public:
  DevicePolicyObserver()
      : pref_observer_(
            crosapi::mojom::PrefPath::kDeviceSystemWideTracingEnabled,
            base::BindRepeating(&DevicePolicyObserver::OnPrefChanged,
                                base::Unretained(this))) {}

  bool system_wide_tracing_enabled() const {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    return system_wide_tracing_enabled_;
  }

  static const DevicePolicyObserver& GetInstance() {
    static base::NoDestructor<DevicePolicyObserver> instance;
    return *instance;
  }

 private:
  void OnPrefChanged(base::Value value) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    system_wide_tracing_enabled_ = value.GetBool();
  }

  ~DevicePolicyObserver() = default;

  CrosapiPrefObserver pref_observer_;
  bool system_wide_tracing_enabled_ = false;
};
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

ChromeTracingDelegate::BackgroundTracingStateManager::
    BackgroundTracingStateManager() = default;
ChromeTracingDelegate::BackgroundTracingStateManager::
    ~BackgroundTracingStateManager() = default;

ChromeTracingDelegate::BackgroundTracingStateManager&
ChromeTracingDelegate::BackgroundTracingStateManager::GetInstance() {
  static base::NoDestructor<BackgroundTracingStateManager> instance;
  return *instance;
}

void ChromeTracingDelegate::BackgroundTracingStateManager::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (initialized_)
    return;
  initialized_ = true;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  const base::Value* dict =
      local_state->GetDictionary(prefs::kBackgroundTracingSessionState);
  if (!dict) {
    SaveState();
    return;
  }
  absl::optional<int> state = dict->FindIntKey(kTracingStateKey);
  if (state) {
    if (*state >= 0 &&
        *state <= static_cast<int>(BackgroundTracingState::LAST)) {
      last_session_end_state_ = static_cast<BackgroundTracingState>(*state);
    } else {
      last_session_end_state_ = BackgroundTracingState::NOT_ACTIVATED;
    }
  }

  const base::Value* upload_times = dict->FindListKey(kUploadTimesKey);
  if (upload_times) {
    for (const auto& scenario_dict : upload_times->GetListDeprecated()) {
      DCHECK(scenario_dict.is_dict());
      const std::string* scenario = scenario_dict.FindStringKey(kScenarioKey);
      const base::Value* timestamp_val =
          scenario_dict.FindKey(kUploadTimestampKey);
      if (!scenario || !timestamp_val) {
        continue;
      }
      absl::optional<base::Time> upload_time = base::ValueToTime(timestamp_val);
      if (!upload_time) {
        continue;
      }
      if ((base::Time::Now() - *upload_time) >
          base::Days(kMinDaysUntilNextUpload)) {
        continue;
      }
      scenario_last_upload_timestamp_[*scenario] = *upload_time;
    }
  }

  // Save state to update the current session state, replacing the previous
  // session state.
  SaveState();
}

void ChromeTracingDelegate::BackgroundTracingStateManager::SaveState() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
  SaveState(scenario_last_upload_timestamp_, state_);
}

// static
void ChromeTracingDelegate::BackgroundTracingStateManager::SaveState(
    const ChromeTracingDelegate::ScenarioUploadTimestampMap&
        scenario_upload_times,
    ChromeTracingDelegate::BackgroundTracingState state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetIntKey(kTracingStateKey, static_cast<int>(state));
  base::Value upload_times(base::Value::Type::LIST);
  for (const auto& it : scenario_upload_times) {
    base::Value scenario(base::Value::Type::DICTIONARY);
    scenario.SetStringKey(kScenarioKey, StripScenarioName(it.first));
    scenario.SetKey(kUploadTimestampKey, base::TimeToValue(it.second));
    upload_times.Append(std::move(scenario));
  }
  dict.SetKey(kUploadTimesKey, std::move(upload_times));

  PrefService* local_state = g_browser_process->local_state();
  local_state->Set(prefs::kBackgroundTracingSessionState, std::move(dict));
  local_state->CommitPendingWrite();
}

bool ChromeTracingDelegate::BackgroundTracingStateManager::
    DidLastSessionEndUnexpectedly() const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
  switch (last_session_end_state_) {
    case BackgroundTracingState::NOT_ACTIVATED:
    case BackgroundTracingState::RAN_30_SECONDS:
    case BackgroundTracingState::FINALIZATION_STARTED:
      return false;
    case BackgroundTracingState::STARTED:
      // If Chrome did not run for 30 seconds after tracing started in previous
      // session then do not start tracing in current session as a safeguard.
      // This would be impacted by short sessions (eg: on Android), but worth
      // the tradeoff of crashing loop on startup. Checking for previous session
      // crash status is platform dependent and the crash status is initialized
      // at later point than when tracing begins. So, this check is safer than
      // waiting for crash metrics to be available. Note that this setting only
      // checks for last session and not sessions before that. So, the next
      // session might still crash due to tracing if the user has another
      // tracing experiment. But, meanwhile we would be able to turn off tracing
      // experiments based on uploaded crash metrics.
      return true;
  }
}

bool ChromeTracingDelegate::BackgroundTracingStateManager::
    DidRecentlyUploadForScenario(
        const content::BackgroundTracingConfig& config) const {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
  std::string stripped_scenario_name =
      StripScenarioName(config.scenario_name());
  auto it = scenario_last_upload_timestamp_.find(stripped_scenario_name);
  if (it != scenario_last_upload_timestamp_.end()) {
    return (base::Time::Now() - it->second) <=
           base::Days(kMinDaysUntilNextUpload);
  }
  return false;
}

void ChromeTracingDelegate::BackgroundTracingStateManager::SetState(
    BackgroundTracingState new_state) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);
  if (state_ == new_state) {
    return;
  }
  // If finalization started before 30 seconds, skip recording the new state.
  if (new_state == BackgroundTracingState::RAN_30_SECONDS &&
      state_ == BackgroundTracingState::FINALIZATION_STARTED) {
    return;
  }
  state_ = new_state;
  SaveState();
}

void ChromeTracingDelegate::BackgroundTracingStateManager::OnScenarioUploaded(
    const std::string& scenario_name) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(initialized_);

  scenario_last_upload_timestamp_[StripScenarioName(scenario_name)] =
      base::Time::Now();
  SaveState();
}

void ChromeTracingDelegate::RegisterPrefs(PrefRegistrySimple* registry) {
  // TODO(ssid): This is no longer used, remove the pref once the new one is
  // stable.
  registry->RegisterInt64Pref(prefs::kBackgroundTracingLastUpload, 0);
  registry->RegisterDictionaryPref(prefs::kBackgroundTracingSessionState);
}

ChromeTracingDelegate::ChromeTracingDelegate() {
  // Ensure that this code is called on the UI thread, except for
  // tests where a UI thread might not have been initialized at this point.
  DCHECK(
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI) ||
      !content::BrowserThread::IsThreadInitialized(content::BrowserThread::UI));
#if !BUILDFLAG(IS_ANDROID)
  BrowserList::AddObserver(this);
#else
  TabModelList::AddObserver(this);
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // This sets up the pref observer.
  DevicePolicyObserver::GetInstance();
#endif
}

ChromeTracingDelegate::~ChromeTracingDelegate() {
  CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
#if !BUILDFLAG(IS_ANDROID)
  BrowserList::RemoveObserver(this);
#else
  TabModelList::RemoveObserver(this);
#endif
}

#if BUILDFLAG(IS_ANDROID)
void ChromeTracingDelegate::OnTabModelAdded() {
  for (const TabModel* model : TabModelList::models()) {
    if (model->GetProfile()->IsOffTheRecord())
      incognito_launched_ = true;
  }
}

void ChromeTracingDelegate::OnTabModelRemoved() {}

#else

void ChromeTracingDelegate::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->IsOffTheRecord())
    incognito_launched_ = true;
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ChromeTracingDelegate::IsActionAllowed(
    BackgroundScenarioAction action,
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data,
    bool ignore_trace_limit) const {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced and uploaded.
  if (IsBackgroundTracingCommandLine())
    return true;

  if (requires_anonymized_data &&
      (incognito_launched_ || chrome::IsOffTheRecordSessionActive())) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kIncognitoLaunched);
    return false;
  }

  BackgroundTracingStateManager& state =
      BackgroundTracingStateManager::GetInstance();

  // Don't start a new trace if the previous trace did not end.
  if (action == BackgroundScenarioAction::kStartTracing &&
      state.DidLastSessionEndUnexpectedly()) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kLastTracingSessionDidNotEnd);
    return false;
  }

  // Check the trace limit for both kStartTracing and kUploadTrace actions
  // because there is no point starting a trace that can't be uploaded.
  if (!ignore_trace_limit && state.DidRecentlyUploadForScenario(config)) {
    RecordDisallowedMetric(
        TracingFinalizationDisallowedReason::kTraceUploadedRecently);
    return false;
  }

  return true;
}

bool ChromeTracingDelegate::IsAllowedToBeginBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data) {
  // We call Initialize() only when a tracing scenario tries to start, and
  // unless this happens we never save state. In particular, if the background
  // tracing experiment is disabled, Initialize() will never be called, and we
  // will thus not save state. This means that when we save the background
  // tracing session state for one session, and then later read the state in a
  // future session, there might have been sessions between these two where
  // tracing was disabled. Therefore, when IsActionAllowed records
  // TracingFinalizationDisallowedReason::kLastTracingSessionDidNotEnd, it
  // might not be the directly preceding session, but instead it is the
  // previous session where tracing was enabled.
  BackgroundTracingStateManager& state =
      BackgroundTracingStateManager::GetInstance();
  state.Initialize();

  // If the config includes a crash scenario, ignore the trace limit so that a
  // trace can be taken on crash. We check if the trigger is actually due to a
  // crash later before uploading.
  const bool ignore_trace_limit = config.has_crash_scenario();

  if (!IsActionAllowed(BackgroundScenarioAction::kStartTracing, config,
                       requires_anonymized_data, ignore_trace_limit)) {
    return false;
  }

  state.SetState(BackgroundTracingState::STARTED);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce([]() {
        BackgroundTracingStateManager::GetInstance().SetState(
            BackgroundTracingState::RAN_30_SECONDS);
      }),
      base::Seconds(30));
  return true;
}

bool ChromeTracingDelegate::IsAllowedToEndBackgroundScenario(
    const content::BackgroundTracingConfig& config,
    bool requires_anonymized_data,
    bool is_crash_scenario) {
  BackgroundTracingStateManager& state =
      BackgroundTracingStateManager::GetInstance();
  state.SetState(BackgroundTracingState::FINALIZATION_STARTED);

  // If a crash scenario triggered, ignore the trace upload limit and continue
  // uploading.
  const bool ignore_trace_limit = is_crash_scenario;

  if (!IsActionAllowed(BackgroundScenarioAction::kUploadTrace, config,
                       requires_anonymized_data, ignore_trace_limit)) {
    return false;
  }

  state.OnScenarioUploaded(config.scenario_name());
  return true;
}

bool ChromeTracingDelegate::IsSystemWideTracingEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Always allow system tracing in dev mode images.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSystemDevMode)) {
    return true;
  }
  // In non-dev images, honor the pref for system-wide tracing.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  return local_state->GetBoolean(
      chromeos::prefs::kDeviceSystemWideTracingEnabled);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // This is a temporary solution that observes the ash pref
  // chromeos::prefs::kDeviceSystemWideTracingEnabled via mojo IPC provided by
  // CrosapiPrefObserver.
  // crbug.com/1201582 is a long term solution for this which assumes that
  // device flags will be available to Lacros.
  return DevicePolicyObserver::GetInstance().system_wide_tracing_enabled();
#else
  return false;
#endif
}

absl::optional<base::Value> ChromeTracingDelegate::GenerateMetadataDict() {
  base::Value metadata_dict(base::Value::Type::DICTIONARY);
  std::vector<std::string> variations;
  variations::GetFieldTrialActiveGroupIdsAsStrings(base::StringPiece(),
                                                   &variations);

  base::Value variations_list(base::Value::Type::LIST);
  for (const auto& it : variations)
    variations_list.Append(it);

  metadata_dict.SetKey("field-trials", std::move(variations_list));
  metadata_dict.SetStringKey("revision", version_info::GetLastChange());
  return metadata_dict;
}
