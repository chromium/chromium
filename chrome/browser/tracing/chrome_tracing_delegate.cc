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
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/tracing/common/background_tracing_state_manager.h"
#include "components/tracing/common/background_tracing_utils.h"
#include "components/tracing/common/tracing_scenarios_config.h"
#include "components/variations/active_field_trials.h"
#include "components/version_info/version_info.h"
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

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_pref_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "base/task/thread_pool.h"
#include "chrome/installer/util/system_tracing_util.h"
#endif

namespace {

using tracing::BackgroundTracingStateManager;

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
void ChromeTracingDelegate::OnTabModelAdded(TabModel* tab_model) {
  for (const TabModel* model : TabModelList::models()) {
    if (model->GetProfile()->IsOffTheRecord())
      incognito_launched_ = true;
  }
}

void ChromeTracingDelegate::OnTabModelRemoved(TabModel* tab_model) {}

#else

void ChromeTracingDelegate::OnBrowserAdded(Browser* browser) {
  if (browser->profile()->IsOffTheRecord())
    incognito_launched_ = true;
}
#endif  // BUILDFLAG(IS_ANDROID)

bool ChromeTracingDelegate::IsRecordingAllowed(
    bool requires_anonymized_data) const {
  // If the background tracing is specified on the command-line, we allow
  // any scenario to be traced and uploaded.
  if (tracing::IsBackgroundTracingEnabledFromCommandLine()) {
    return true;
  }

  if (requires_anonymized_data &&
      (incognito_launched_ || IsOffTheRecordSessionActive())) {
    UMA_HISTOGRAM_ENUMERATION(
        "Tracing.Background.FinalizationDisallowedReason",
        TracingFinalizationDisallowedReason::kIncognitoLaunched);
    return false;
  }

  return true;
}

bool ChromeTracingDelegate::ShouldSaveUnuploadedTrace() const {
  return true;
}

#if BUILDFLAG(IS_WIN)
void ChromeTracingDelegate::GetSystemTracingState(
    base::OnceCallback<void(bool service_supported, bool service_enabled)>
        on_tracing_state) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock{}},
      base::BindOnce([]() -> std::pair<bool, bool> {
        return {installer::IsSystemTracingServiceSupported(),
                installer::IsSystemTracingServiceRegistered()};
      }),
      base::BindOnce(
          [](base::OnceCallback<void(bool service_supported,
                                     bool service_enabled)> on_tracing_state,
             std::pair<bool, bool> state) {
            std::move(on_tracing_state).Run(state.first, state.second);
          },
          std::move(on_tracing_state)));
}

void ChromeTracingDelegate::EnableSystemTracing(
    base::OnceCallback<void(bool success)> on_complete) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock{}}, base::BindOnce([]() {
        return installer::ElevateAndRegisterSystemTracingService();
      }),
      std::move(on_complete));
}

void ChromeTracingDelegate::DisableSystemTracing(
    base::OnceCallback<void(bool success)> on_complete) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock{}}, base::BindOnce([]() {
        return installer::ElevateAndDeregisterSystemTracingService();
      }),
      std::move(on_complete));
}
#endif  // BUILDFLAG(IS_WIN)

bool ChromeTracingDelegate::IsSystemWideTracingEnabled() {
#if BUILDFLAG(IS_CHROMEOS)
  // Always allow system tracing in dev mode images.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kSystemDevMode)) {
    return true;
  }
  // In non-dev images, honor the pref for system-wide tracing.
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  return local_state->GetBoolean(ash::prefs::kDeviceSystemWideTracingEnabled);
#else
  return false;
#endif
}
