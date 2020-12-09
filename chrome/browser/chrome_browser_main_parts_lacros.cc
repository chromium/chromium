// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_main_parts_lacros.h"

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lacros/metrics_reporting_observer.h"
#include "content/public/common/result_codes.h"

ChromeBrowserMainPartsLacros::ChromeBrowserMainPartsLacros(
    const content::MainFunctionParams& parameters,
    StartupData* startup_data)
    : ChromeBrowserMainPartsLinux(parameters, startup_data) {}

ChromeBrowserMainPartsLacros::~ChromeBrowserMainPartsLacros() = default;

int ChromeBrowserMainPartsLacros::PreEarlyInitialization() {
  int result = ChromeBrowserMainPartsLinux::PreEarlyInitialization();
  if (result != content::RESULT_CODE_NORMAL_EXIT)
    return result;

  // The observer sets the initial metrics consent state, then observes ash
  // for updates. Create it here because local state is required to check for
  // policy overrides.
  DCHECK(g_browser_process->local_state());
  metrics_reporting_observer_ = std::make_unique<MetricsReportingObserver>(
      g_browser_process->local_state());
  metrics_reporting_observer_->Init();

  return content::RESULT_CODE_NORMAL_EXIT;
}
