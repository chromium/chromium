// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "components/reporting/client/report_queue_provider.h"

namespace reporting {

// This class is used to report events when a user it added or removed to a
// managed device. It will report this when the policy kReportDeviceUsers is
// true. Unaffiliated users being added will be reported, but their email
// will be omitted.
class UserAddedRemovedReporter
    : public policy::ManagedSessionService::Observer {
 public:
  // For prod. Uses the default implementation of UserEventReporterHelper.
  static std::unique_ptr<UserAddedRemovedReporter> Create(
      base::flat_map<AccountId, bool> users_to_be_removed,
      policy::ManagedSessionService* managed_session_service);

  // For use in testing only. Allows user to pass in a test helper.
  static std::unique_ptr<UserAddedRemovedReporter> CreateForTesting(
      std::unique_ptr<UserEventReporterHelper> helper,
      base::flat_map<AccountId, bool> users_to_be_removed,
      policy::ManagedSessionService* managed_session_service);

  // Returns whether the removal of the user identified by `account_id` is
  // a target of the reporting. Guest, device local accounts and deprecated
  // accounts (deprecated ARC kiosk accounts and legacy supervised users) are
  // not.
  // Note that this needs to work without looking up the User instance, because
  // the User may be already gone when the removal is processed, e.g., the
  // removal is processed on start up.
  static bool IsReportableUserRemoval(const AccountId& account_id);

  UserAddedRemovedReporter(const UserAddedRemovedReporter& other) = delete;
  UserAddedRemovedReporter& operator=(const UserAddedRemovedReporter& other) =
      delete;

  ~UserAddedRemovedReporter() override;

  // Processes the removed user. `user_email` can be empty if the user's email
  // should not be reported (e.g. unaffiliated user). The removal event is
  // still reported in that case. Whether the reporting is enabled is checked
  // when the event is actually reported, after the device settings become
  // trusted.
  void ProcessRemovedUser(std::string_view user_email,
                          user_manager::UserRemovalReason reason);

  // ManagedSessionService::Observer overrides.
  // Check if login was to a new account.
  void OnLogin(Profile* profile) override;
  // Track that a user will be removed. This should be done before users are
  // removed so that it can be checked if they are affiliated.
  void OnUserToBeRemoved(const AccountId& account_id) override;
  // Report that a user has been removed.
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override;

 private:
  UserAddedRemovedReporter(
      std::unique_ptr<UserEventReporterHelper> helper,
      base::flat_map<AccountId, bool> users_to_be_removed,
      policy::ManagedSessionService* managed_session_service);

  // Reports the removed user, if the reporting is enabled. If the device
  // settings are not yet trusted, this waits for it.
  void ReportRemovedUser(std::string user_email,
                         user_manager::UserRemovalReason reason);

  std::unique_ptr<UserEventReporterHelper> helper_;

  // Maps a user's email to if they are affiliated. This is needed to determine
  // if their email may be reported.
  base::flat_map<AccountId, bool> users_to_be_removed_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  base::WeakPtrFactory<UserAddedRemovedReporter> weak_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_
