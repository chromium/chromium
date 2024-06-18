// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_OS_UPDATES_OS_UPDATES_REPORTER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_OS_UPDATES_OS_UPDATES_REPORTER_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/sequence_checker.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/os_events.pb.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/update_engine_client.h"

namespace reporting {

// This class is used to report events related to the OS of a device.
// It will report when an update was finished successfully or when it encounters
// an error in the process. Also reports when a powerwash is initiated on
// the device either by the user or the admin.
class OsUpdatesReporter : public ash::UpdateEngineClient::Observer,
                          public ash::SessionManagerClient::Observer {
 public:
  // Interface for observing OS update failures for event based log upload. The
  // observer lifetime is managed by `EventBasedLogManager`.
  class OsUpdateEventBasedLogObserver : public base::CheckedObserver {
   public:
    // Called when update failure is detected and will trigger log
    // upload with `upload_id`.
    virtual void OnOsUpdateFailed(std::string upload_id) = 0;
  };

  // For prod. Uses the default implementation of UserEventReporterHelper.
  static std::unique_ptr<OsUpdatesReporter> Create();

  // For use in testing only. Allows user to pass in a test helper.
  static std::unique_ptr<OsUpdatesReporter> CreateForTesting(
      std::unique_ptr<UserEventReporterHelper> helper);

  OsUpdatesReporter(const OsUpdatesReporter&) = delete;
  OsUpdatesReporter& operator=(const OsUpdatesReporter&) = delete;

  ~OsUpdatesReporter() override;

  void AddObserver(OsUpdateEventBasedLogObserver* observer);
  void RemoveObserver(const OsUpdateEventBasedLogObserver* observer);

  // ash::UpdateEngineClient::Observer:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;
  // ash::SessionManagerClient::Observer:
  void PowerwashRequested(bool remote_request) override;

 private:
  explicit OsUpdatesReporter(std::unique_ptr<UserEventReporterHelper> helper);

  void MaybeReportEvent(ash::reporting::OsEventsRecord record);

  // Notifies `OsUpdateEventBasedLogObserver` that the update failed. Returns
  // the upload id attached to the triggered upload if any observer is notified.
  // Returns nullopt if no observers are notified.
  std::optional<std::string> NotifyOsUpdateFailed();

  base::ObserverList<OsUpdateEventBasedLogObserver> observers_;

  std::unique_ptr<UserEventReporterHelper> helper_;

  // Stores the last powerwash attempt time to eliminate duplicates.
  base::Time last_powerwash_attempt_time_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_OS_UPDATES_OS_UPDATES_REPORTER_H_
