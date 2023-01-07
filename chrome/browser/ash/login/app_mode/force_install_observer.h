// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_FORCE_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_FORCE_INSTALL_OBSERVER_H_

#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/force_installed_tracker_ash.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"

class Profile;

namespace app_mode {

// Class that observes the installation of forced extensions for kiosks.
//
// Based on the enabled feature flags, this class either observes the
// installation in Lacros or Ash Chrome.
class ForceInstallObserver
    : public extensions::ForceInstalledTracker::Observer {
 public:
  enum class Result { kSuccess, kTimeout, kInvalidPolicy };
  using ResultCallback = base::OnceCallback<void(Result)>;

  ForceInstallObserver(Profile* profile, ResultCallback callback);
  ~ForceInstallObserver() override;

 private:
  // ForceInstalledTracker::Observer:
  void OnForceInstalledExtensionsReady() override;
  void OnForceInstalledExtensionFailed(
      const extensions::ExtensionId& extension_id,
      extensions::InstallStageTracker::FailureReason reason,
      bool is_from_store) override;

  void StartObservingAsh(Profile* profile);
  void StartObservingLacros();

  void StartTimerToWaitForExtensions();
  void OnExtensionWaitTimeOut();

  void ReportDone();
  void ReportTimeout();
  void ReportInvalidPolicy();

  // A timer that fires when the force-installed extensions were not ready
  // within the allocated time.
  base::OneShotTimer installation_wait_timer_;

  // Tracks the moment when extensions start to be installed.
  base::Time installation_start_time_;

  // Callback that is used to indicate to the caller that the extension install
  // has completed.
  ResultCallback callback_;
  base::ScopedObservation<extensions::ForceInstalledTracker,
                          extensions::ForceInstalledTracker::Observer>
      observation_for_ash_{this};
  base::ScopedObservation<crosapi::ForceInstalledTrackerAsh,
                          extensions::ForceInstalledTracker::Observer>
      observation_for_lacros_{this};
};

}  // namespace app_mode

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_FORCE_INSTALL_OBSERVER_H_
