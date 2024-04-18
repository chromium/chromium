// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_

#include <memory>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_encrypted_event_reporter.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_manager.h"
#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/app_install_events.pb.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class Profile;

namespace policy {
BASE_DECLARE_FEATURE(kUseEncryptedReportingPipelineToReportArcAppInstallEvents);

// Observes the pref that indicates whether to log events for app push-installs.
// When logging is enabled, creates an |AppInstallEventLogManager|. When logging
// is disabled, destroys the |AppInstallEventLogManager|, if any, and clears all
// data related to the app-install event log. Ensures correct sequencing of I/O
// operations by using one |AppInstallEventLogManager::LogTaskRunnerWrapper| for
// all accesses to the log file.
class AppInstallEventLogManagerWrapper {
 public:
  AppInstallEventLogManagerWrapper(const AppInstallEventLogManagerWrapper&) =
      delete;
  AppInstallEventLogManagerWrapper& operator=(
      const AppInstallEventLogManagerWrapper&) = delete;

  virtual ~AppInstallEventLogManagerWrapper();

  // Creates a new |AppInstallEventLogManager| to handle app push-install event
  // logging for |profile|. The object returned manages its own lifetime and
  // self-destructs on logout.
  static AppInstallEventLogManagerWrapper* CreateForProfile(Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 protected:
  explicit AppInstallEventLogManagerWrapper(Profile* profile);

  // Must be called right after construction. Extracted into a separate method
  // for testing.
  void Init();

  // Initializes log collection.
  void InitLogging();

  // Destructs all logging-related objects.
  void DisableLogging();

  // Creates the |log_manager_|. Virtual for testing.
  virtual void CreateManager();

  // Destroys the |log_manager_|. Virtual for testing.
  virtual void DestroyManager();

  // Creates the |encrypted_reporter_|. Virtual for testing.
  virtual void CreateEncryptedReporter();

  // Destroys the |encrypted_reporter_|. Virtual for testing.
  virtual void DestroyEncryptedReporter();

  // Provides the task runner used for all I/O on the log file.
  std::unique_ptr<ArcAppInstallEventLogManager::LogTaskRunnerWrapper>
      log_task_runner_;

 private:
  // Holds the value of the
  // `kUseEncryptedReportingPipelineToReportArcAppInstallEvents` feature.
  const bool use_encrypted_reporting_pipeline_;

  // Evaluates the current state of the pref that indicates whether to log
  // events for app push-installs. If logging is enabled, creates the
  // |log_manager_|. If logging is disabled, destroys the |log_manager_| and
  // clears all data related to the app-install event log.
  void EvaluatePref();

  // Callback invoked when Chrome shuts down.
  void OnAppTerminating();

  // The profile whose app push-install events are being logged.
  const raw_ptr<Profile> profile_;

  // Handles collection, storage and upload of app push-install event logs.
  std::unique_ptr<ArcAppInstallEventLogManager> log_manager_;

  std::unique_ptr<ArcAppInstallEncryptedEventReporter> encrypted_reporter_;

  // Pref change observer.
  PrefChangeRegistrar pref_change_registrar_;

  // Browser shutdown callback, used to destroy the |log_manager_| when the
  // user is logging out, giving it an opportunity to log the event.
  base::CallbackListSubscription on_app_terminating_subscription_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_APP_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
