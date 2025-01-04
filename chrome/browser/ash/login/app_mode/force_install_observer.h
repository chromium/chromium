// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_FORCE_INSTALL_OBSERVER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_FORCE_INSTALL_OBSERVER_H_

#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/forced_extensions/force_installed_tracker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace app_mode {

// Class that observes the installation of forced extensions for kiosks.
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

  void StartObserving(Profile* profile);

  void StartTimerToWaitForExtensions();
  void OnExtensionWaitTimeOut();

  void ReportDone();
  void ReportTimeout();
  void ReportInvalidPolicy();

  // Timeout to wait for force-installed extensions to be ready.
  base::OneShotTimer installation_wait_timer_;

  // Tracks the moment when extensions start to be installed.
  base::Time installation_start_time_;

  // Callback to run when extension install is complete.
  ResultCallback callback_;

  base::ScopedObservation<extensions::ForceInstalledTracker,
                          extensions::ForceInstalledTracker::Observer>
      observation_{this};
};

}  // namespace app_mode

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_FORCE_INSTALL_OBSERVER_H_
