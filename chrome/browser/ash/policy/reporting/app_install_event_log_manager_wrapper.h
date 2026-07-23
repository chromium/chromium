// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_encrypted_event_reporter.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/app_install_events.pb.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class PrefService;
class Profile;

namespace policy {

// Observes the pref that indicates whether to log events for app push-installs.
// When logging is enabled, creates an |AppInstallEventLogManager|. When logging
// is disabled, destroys the |AppInstallEventLogManager|, if any, and clears all
// data related to the app-install event log. Ensures correct sequencing of I/O
// operations by using one |AppInstallEventLogManager::LogTaskRunnerWrapper| for
// all accesses to the log file.
class AppInstallEventLogManagerWrapper
    : public ash::SessionTerminationManager::Observer {
 public:
  AppInstallEventLogManagerWrapper(const AppInstallEventLogManagerWrapper&) =
      delete;
  AppInstallEventLogManagerWrapper& operator=(
      const AppInstallEventLogManagerWrapper&) = delete;

  ~AppInstallEventLogManagerWrapper() override;

  // Creates a new `AppInstallEventLogManager` to handle app push-install event
  // logging for `profile`. The object created manages its own lifetime and
  // self-destructs on logout.
  // `local_state` must be non-null and must be alive while the main RunLoop is
  // running.
  // TODO(crbug.com/530040110): Refactor the lifetime and return a unique_ptr.
  static void CreateForProfile(PrefService* local_state, Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 protected:
  // `local_state` must be non-null and must outlive `this`.
  AppInstallEventLogManagerWrapper(PrefService* local_state, Profile* profile);

  // Must be called right after construction. Extracted into a separate method
  // for testing.
  void Init();

  // Initializes log collection.
  void InitLogging();

  // Destructs all logging-related objects.
  void DisableLogging();

  // Creates the |encrypted_reporter_|. Virtual for testing.
  virtual void CreateEncryptedReporter();

  // Destroys the |encrypted_reporter_|. Virtual for testing.
  virtual void DestroyEncryptedReporter();

 private:
  // Evaluates the current state of the pref that indicates whether to log
  // events for app push-installs. If logging is enabled, creates the
  // |encrypted_reporter_|. If logging is disabled, destroys the
  // |encrypted_reporter_| and clears all data related to the app-install event
  // log.
  void EvaluatePref();

  // ash::SessionTerminationManager::Observer:
  void OnAppTerminating() override;

  const raw_ref<PrefService> local_state_;

  // The profile whose app push-install events are being logged.
  const raw_ptr<Profile> profile_;

  base::ScopedObservation<ash::SessionTerminationManager,
                          ash::SessionTerminationManager::Observer>
      session_termination_observation_{this};

  std::unique_ptr<ArcAppInstallEncryptedEventReporter> encrypted_reporter_;

  // Pref change observer.
  PrefChangeRegistrar pref_change_registrar_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
