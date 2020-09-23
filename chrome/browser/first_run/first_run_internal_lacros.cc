// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "build/branding_buildflags.h"
#include "chrome/browser/first_run/first_run_dialog.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace first_run {
namespace internal {

void DoPostImportPlatformSpecificTasks(Profile* profile) {
  const crosapi::mojom::LacrosInitParams* init_params =
      chromeos::LacrosChromeServiceImpl::Get()->init_params();
  // For old versions of ash that don't send the metrics state, always show
  // the first run dialog. We don't worry about policy control because lacros
  // doesn't support that yet, and this code will be removed before policy
  // support is added.
  // TODO(https://crbug.com/1131164): Remove after M87 beta, when all supported
  // ash versions will set |ash_metrics_enabled| true or false.
  if (!init_params->ash_metrics_enabled_has_value) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    ShowFirstRunDialog(profile);
#endif
    return;
  }
  // Lacros skips the first run dialog because Chrome is the default browser on
  // Chrome OS and metrics consent is chosen during the Chrome OS out of box
  // setup experience. Lacros inherits first-run metrics consent from ash over
  // mojo. After first-run lacros handles metrics consent via settings.
  ChangeMetricsReportingState(init_params->ash_metrics_enabled);
}

bool ShowPostInstallEULAIfNeeded(installer::InitialPreferences* install_prefs) {
  // Just continue. The EULA is only used on Windows.
  return true;
}

}  // namespace internal
}  // namespace first_run
