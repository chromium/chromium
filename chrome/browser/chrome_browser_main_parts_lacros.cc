// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_parts_lacros.h"

#include "base/check.h"
#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/metrics_reporting_observer.h"
#include "chrome/browser/lacros/prefs_ash_observer.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/lacros/dbus/lacros_dbus_helper.h"
#include "chromeos/startup/browser_params_proxy.h"
#include "content/public/browser/tts_platform.h"
#include "content/public/common/result_codes.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/wm/core/wm_core_switches.h"

ChromeBrowserMainPartsLacros::ChromeBrowserMainPartsLacros(
    bool is_integration_test,
    StartupData* startup_data)
    : ChromeBrowserMainPartsLinux(is_integration_test, startup_data) {}

ChromeBrowserMainPartsLacros::~ChromeBrowserMainPartsLacros() = default;

int ChromeBrowserMainPartsLacros::PreEarlyInitialization() {
  int result = ChromeBrowserMainPartsLinux::PreEarlyInitialization();
  if (result != content::RESULT_CODE_NORMAL_EXIT)
    return result;

  // The observer sets the initial metrics consent state, then observes ash
  // for updates. Create it here because local state is required to check for
  // policy overrides.
  MetricsReportingObserver::InitSettingsFromAsh();

  prefs_ash_observer_ =
      std::make_unique<PrefsAshObserver>(g_browser_process->local_state());
  prefs_ash_observer_->Init();

  return content::RESULT_CODE_NORMAL_EXIT;
}

int ChromeBrowserMainPartsLacros::PreCreateThreads() {
  const chromeos::BrowserParamsProxy* init_params =
      chromeos::BrowserParamsProxy::Get();
  if (init_params->InitialBrowserAction() ==
      crosapi::mojom::InitialBrowserAction::kDoNotOpenWindow) {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kNoStartupWindow);
  }
  return ChromeBrowserMainPartsLinux::PreCreateThreads();
}

void ChromeBrowserMainPartsLacros::PostCreateThreads() {
  if (g_browser_process->metrics_service()) {
    metrics_reporting_observer_ = MetricsReportingObserver::CreateObserver(
        g_browser_process->metrics_service());
  } else {
    LOG(WARNING)
        << "Metrics service is not available, not syncing metrics settings.";
  }
  return ChromeBrowserMainPartsLinux::PostCreateThreads();
}

void ChromeBrowserMainPartsLacros::PreProfileInit() {
  ChromeBrowserMainPartsLinux::PreProfileInit();

  // Apply specific flags if this is a Kiosk session.
  if (chromeos::BrowserParamsProxy::Get()->SessionType() ==
          crosapi::mojom::SessionType::kWebKioskSession ||
      chromeos::BrowserParamsProxy::Get()->SessionType() ==
          crosapi::mojom::SessionType::kAppKioskSession) {
    // Hide certain system UI elements.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kForceAppMode);

    // Disable window animation since kiosk app runs in a single full screen
    // window and window animation causes start-up janks.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        wm::switches::kWindowAnimationsDisabled);
  }

  // Initialize TtsPlatform so that TtsPlatformImplLacros can observe the
  // ProfileManager for OnProfileAdded event before the profile is loaded.
  content::TtsPlatform::GetInstance();
}

void ChromeBrowserMainPartsLacros::PostProfileInit(Profile* profile,
                                                   bool is_initial_profile) {
  ChromeBrowserMainPartsLinux::PostProfileInit(profile, is_initial_profile);
  prefs_ash_observer_->InitPostProfileInitialized(profile);
}

void ChromeBrowserMainPartsLacros::PostMainMessageLoopRun() {
  // Reset MetricsReportingObserver here to guarantee it's destroyed before
  // `g_browser_process->metrics_service()` is destructed as
  // MetricsReportingObserver depends on metrics service.
  metrics_reporting_observer_.reset();

  ChromeBrowserMainParts::PostMainMessageLoopRun();

  ui::OzonePlatform::GetInstance()->PostMainMessageLoopRun();
}

void ChromeBrowserMainPartsLacros::PostDestroyThreads() {
  chromeos::LacrosShutdownDBus();

  // Reset PrefsAshObserver here to guarantee it's destroyed before
  // `g_browser_process->local_state()` is destructed as PrefsAshObserver
  // depends on local state.
  prefs_ash_observer_.reset();

  ChromeBrowserMainPartsLinux::PostDestroyThreads();
}
