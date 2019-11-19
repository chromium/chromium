// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "chrome/installer/util/master_preferences.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/prefs/pref_service.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"

namespace first_run {

#if !defined(OS_CHROMEOS)
base::OnceClosure& GetBeforeShowFirstRunDialogHookForTesting() {
  static base::NoDestructor<base::OnceClosure> closure;
  return *closure;
}
#endif  // OS_CHROMEOS

namespace internal {
namespace {

enum class ForcedShowDialogState {
  kNotForced,
  kForceShown,
  kForceSuppressed,
};

ForcedShowDialogState g_forced_show_dialog_state =
    ForcedShowDialogState::kNotForced;

#if !defined(OS_CHROMEOS)
// Returns whether the first run dialog should be shown. This is only true for
// certain builds, and only if the user has not already set preferences. In a
// real, official-build first run, initializes the default metrics reporting if
// the dialog should be shown.
bool ShouldShowFirstRunDialog() {
  if (g_forced_show_dialog_state != ForcedShowDialogState::kNotForced)
    return g_forced_show_dialog_state == ForcedShowDialogState::kForceShown;

#if !BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // On non-official builds, only --force-first-run-dialog will show the dialog.
  return false;
#endif

  base::FilePath local_state_path;
  base::PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
  if (base::PathExists(local_state_path))
    return false;

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
  // reporting state when the first run dialog is shown.
  bool is_opt_in = first_run::IsMetricsReportingOptIn();
  metrics::RecordMetricsReportingDefaultState(
      g_browser_process->local_state(),
      is_opt_in ? metrics::EnableMetricsDefault::OPT_IN
                : metrics::EnableMetricsDefault::OPT_OUT);
  return true;
}
#endif  // !OS_CHROMEOS

}  // namespace

void ForceFirstRunDialogShownForTesting(bool shown) {
  if (shown)
    g_forced_show_dialog_state = ForcedShowDialogState::kForceShown;
  else
    g_forced_show_dialog_state = ForcedShowDialogState::kForceSuppressed;
}

void DoPostImportPlatformSpecificTasks(Profile* profile) {
#if !defined(OS_CHROMEOS)
  if (!ShouldShowFirstRunDialog())
    return;

  if (GetBeforeShowFirstRunDialogHookForTesting())
    std::move(GetBeforeShowFirstRunDialogHookForTesting()).Run();

  ShowFirstRunDialog(profile);
  startup_metric_utils::SetNonBrowserUIDisplayed();
#endif  // !OS_CHROMEOS
}

bool IsFirstRunSentinelPresent() {
  base::FilePath sentinel;
  return !GetFirstRunSentinelFilePath(&sentinel) || base::PathExists(sentinel);
}

bool ShowPostInstallEULAIfNeeded(installer::MasterPreferences* install_prefs) {
  // The EULA is only handled on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run
