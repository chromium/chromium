// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#error "Chrome OS should use first_run_internal_chromeos.cc."
#endif

namespace first_run {

base::OnceClosure& GetBeforeShowFirstRunDialogHookForTesting() {
  static base::NoDestructor<base::OnceClosure> closure;
  return *closure;
}

namespace internal {
namespace {

enum class ForcedShowDialogState {
  kNotForced,
  kForceShown,
  kForceSuppressed,
};

ForcedShowDialogState g_forced_show_dialog_state =
    ForcedShowDialogState::kNotForced;

// Returns whether the first run dialog should be shown. This is only true for
// certain builds, and only if the user has not already set preferences. In a
// real, official-build first run, initializes the default metrics reporting if
// the dialog should be shown.
bool ShouldShowFirstRunDialog() {
  if (g_forced_show_dialog_state != ForcedShowDialogState::kNotForced)
    return g_forced_show_dialog_state == ForcedShowDialogState::kForceShown;

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return false;
#else
  if (!IsOrganicFirstRun())
    return false;

  // The purpose of the dialog is to ask the user to enable stats and crash
  // reporting. This setting may be controlled through configuration management
  // in enterprise scenarios. If that is the case, skip the dialog entirely, as
  // it's not worth bothering the user for only the default browser question
  // (which is likely to be forced in enterprise deployments anyway).
  if (IsMetricsReportingPolicyManaged())
    return false;

  // For real first runs, Mac and Desktop Linux initialize the default metrics
  // reporting state when the first run dialog is shown. These days, metrics are
  // always enabled by default (opt-out).
  metrics::RecordMetricsReportingDefaultState(
      g_browser_process->local_state(), metrics::EnableMetricsDefault::OPT_OUT);
  return true;
#endif
}

}  // namespace

void ForceFirstRunDialogShownForTesting(bool shown) {
  if (shown)
    g_forced_show_dialog_state = ForcedShowDialogState::kForceShown;
  else
    g_forced_show_dialog_state = ForcedShowDialogState::kForceSuppressed;
}

void DoPostImportPlatformSpecificTasks() {
  if (!ShouldShowFirstRunDialog())
    return;

  if (GetBeforeShowFirstRunDialogHookForTesting())
    std::move(GetBeforeShowFirstRunDialogHookForTesting()).Run();

  ShowFirstRunDialog();
  startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();
}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  // The EULA is only handled on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run
