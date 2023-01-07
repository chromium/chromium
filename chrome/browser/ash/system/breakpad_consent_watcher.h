// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_BREAKPAD_CONSENT_WATCHER_H_
#define CHROME_BROWSER_ASH_SYSTEM_BREAKPAD_CONSENT_WATCHER_H_

#include <memory>

#include "chrome/browser/ash/settings/stats_reporting_controller.h"

namespace ash {
namespace system {

// Watches the crash reporting consent setting (cros.metrics.reportingEnabled).
// If crash reporting is enabled, installs Breakpad if it isn't already
// installed.
//
// This class is only created in the main browser process. Unfortunately, that
// means changing the consent setting only installs Breakpad in the browser
// process. Renderer and GPU processes started before consent was given will
// still discard any crashes. This problem will be fixed when we move to
// Crashpad.
class BreakpadConsentWatcher {
 public:
  ~BreakpadConsentWatcher();

  // Create a BreakpadConsentWatcher. Returns nullptr if this process doesn't
  // need a BreakpadConsentWatcher. Returning nullptr is NOT indicitive of an
  // error.
  static std::unique_ptr<BreakpadConsentWatcher> Initialize(
      StatsReportingController* stat_controller);

 private:
  BreakpadConsentWatcher();
  BreakpadConsentWatcher(const BreakpadConsentWatcher&) = delete;
  BreakpadConsentWatcher& operator=(const BreakpadConsentWatcher&) = delete;

  // Callback function. Called whenever the crash reporting consent is changed.
  static void OnConsentChange();

  // Callback function that happens on the
  // GoogleUpdateSettings::CollectStatsConsentTaskRunner() thread. We must be
  // on that thread to avoid races. Also, InitCrashReporter() calls blocking
  // functions so we must be on a blockable thread.
  // Called whenever the crash reporting consent is changed.
  static void OnConsentChangeCollectStatsConsentThread();

  base::CallbackListSubscription subscription_;
};

}  // namespace system
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_BREAKPAD_CONSENT_WATCHER_H_
