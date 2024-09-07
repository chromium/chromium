// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tracing/chrome_tracing_delegate.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/variations/active_field_trials.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/background_tracing_config.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/tracing/public/cpp/tracing_features.h"

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

using tracing::BackgroundTracingSetupMode;
using tracing::BackgroundTracingState;
using tracing::BackgroundTracingStateManager;

bool IsBackgroundTracingCommandLine() {
  auto tracing_mode = tracing::GetBackgroundTracingSetupMode();
  if (tracing_mode == BackgroundTracingSetupMode::kFromJsonConfigFile ||
      tracing_mode == BackgroundTracingSetupMode::kFromProtoConfigFile) {
    return true;
  }
  return false;
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

ChromeTracingDelegate::ChromeTracingDelegate()
    : state_manager_(tracing::BackgroundTracingStateManager::CreateInstance(
          g_browser_process->local_state())) {
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
    bool requires_anonymized_data) const {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced and uploaded.
  if (IsBackgroundTracingCommandLine()) {
    return true;
  }

  if (requires_anonymized_data &&
      (incognito_launched_ || chrome::IsOffTheRecordSessionActive())) {
    tracing::RecordDisallowedMetric(
        tracing::TracingFinalizationDisallowedReason::kIncognitoLaunched);
    return false;
  }

  BackgroundTracingStateManager& state =
      BackgroundTracingStateManager::GetInstance();

  // Don't start a new trace if the previous trace did not end.
  if (action == BackgroundScenarioAction::kStartTracing &&
      state.DidLastSessionEndUnexpectedly()) {
    tracing::RecordDisallowedMetric(
        tracing::TracingFinalizationDisallowedReason::
            kLastTracingSessionDidNotEnd);
    return false;
  }

  return true;
}

bool ChromeTracingDelegate::OnBackgroundTracingActive(
    bool requires_anonymized_data) {
  BackgroundTracingStateManager& state =
      BackgroundTracingStateManager::GetInstance();

  if (!IsActionAllowed(BackgroundScenarioAction::kStartTracing,
                       requires_anonymized_data)) {
    return false;
  }

  state.OnTracingStarted();
  return true;
}

bool ChromeTracingDelegate::OnBackgroundTracingIdle(
    bool requires_anonymized_data) {
  BackgroundTracingStateManager& state =
      BackgroundTracingStateManager::GetInstance();
  state.OnTracingStopped();

  return IsActionAllowed(BackgroundScenarioAction::kUploadTrace,
                         requires_anonymized_data);
}

bool ChromeTracingDelegate::ShouldSaveUnuploadedTrace() const {
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
  return local_state->GetBoolean(ash::prefs::kDeviceSystemWideTracingEnabled);
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // This is a temporary solution that observes the ash pref
  // ash::prefs::kDeviceSystemWideTracingEnabled via mojo IPC provided by
  // CrosapiPrefObserver.
  // crbug.com/1201582 is a long term solution for this which assumes that
  // device flags will be available to Lacros.
  return DevicePolicyObserver::GetInstance().system_wide_tracing_enabled();
#else
  return false;
#endif
}
