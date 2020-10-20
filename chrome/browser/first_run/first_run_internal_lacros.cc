// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/first_run/first_run_internal.h"

#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace first_run {
namespace internal {

void DoPostImportPlatformSpecificTasks(Profile* profile) {
  const crosapi::mojom::LacrosInitParams* init_params =
      chromeos::LacrosChromeServiceImpl::Get()->init_params();
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
