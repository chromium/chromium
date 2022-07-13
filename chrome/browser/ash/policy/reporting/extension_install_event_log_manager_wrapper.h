// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_

#include <memory>

#include "base/callback_list.h"
#include "chrome/browser/ash/policy/reporting/extension_install_event_log_manager.h"
#include "components/prefs/pref_change_registrar.h"

class PrefRegistrySimple;
class Profile;

namespace policy {

// Observes the pref that indicates whether to log events for extension
// installs. When logging is enabled, creates an
// |ExtensionInstallEventLogManager|. When logging is disabled, destroys the
// |ExtensionInstallEventLogManager|, if any, and clears all data related to the
// extension install event log. Ensures correct sequencing of I/O operations by
// using one |InstallEventLogManager::LogTaskRunnerWrapper| for all accesses to
// the log file. An AppTerminatingCallback is used to delete the
// ThreadTaskRunner when the last browser window has been shut down.
class ExtensionInstallEventLogManagerWrapper {
 public:
  virtual ~ExtensionInstallEventLogManagerWrapper();

  // Creates a new |ExtensionInstallEventLogManager| to handle extension install
  // event logging for |profile|. The object returned manages its own lifetime
  // and self-destructs on logout. The reporting is supported only for cloud
  // managed users on ChromeOS. Returns nullptr in other cases.
  static ExtensionInstallEventLogManagerWrapper* CreateForProfile(
      Profile* profile);

  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

 protected:
  explicit ExtensionInstallEventLogManagerWrapper(Profile* profile);

  // Must be called right after construction. Extracted into a separate method
  // for testing.
  void Init();

  // Creates the |log_manager_|. Virtual for testing.
  virtual void CreateManager();

  // Destroys the |log_manager_|. Virtual for testing.
  virtual void DestroyManager();

  // Provides the task runner used for all I/O on the log file.
  std::unique_ptr<ExtensionInstallEventLogManager::LogTaskRunnerWrapper>
      log_task_runner_;

 private:
  // Evaluates the current state of the pref that indicates whether to log
  // events for extension installs. If logging is enabled, creates the
  // |log_manager_|. If logging is disabled, destroys the |log_manager_| and
  // clears all data related to the extension install event log.
  void EvaluatePref();

  void OnAppTerminating();

  // The profile whose extension install events are being logged.
  Profile* const profile_;

  // Handles collection, storage and upload of extension install event logs.
  std::unique_ptr<ExtensionInstallEventLogManager> log_manager_;

  // Pref change observer.
  PrefChangeRegistrar pref_change_registrar_;

  base::CallbackListSubscription app_terminating_subscription_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_MANAGER_WRAPPER_H_
