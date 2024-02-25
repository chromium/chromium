// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_

#include <memory>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/profiles/profile.h"
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

  UserAddedRemovedReporter(const UserAddedRemovedReporter& other) = delete;
  UserAddedRemovedReporter& operator=(const UserAddedRemovedReporter& other) =
      delete;

  ~UserAddedRemovedReporter() override;

  // Check the UserManager removed user cache for users removed before
  // the reporter is created.
  void ProcessRemoveUserCache();

  // Processes the removed user.
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

  std::unique_ptr<UserEventReporterHelper> helper_;

  // Maps a user's email to if they are affiliated. This is needed to determine
  // if their email may be reported.
  base::flat_map<AccountId, bool> users_to_be_removed_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_
