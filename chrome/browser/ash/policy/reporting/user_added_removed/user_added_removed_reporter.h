// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/add_remove_user_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace reporting {

// This class is used to report events when a user it added or removed to a
// managed device. It will report this when the policy kReportDeviceUsers is
// true. Unaffiliated users being added will be reported, but their email
// will be omitted.
class UserAddedRemovedReporter
    : public policy::ManagedSessionService::Observer {
 public:
  // For use in testing only. Allows user to pass in a test helper.
  explicit UserAddedRemovedReporter(
      std::unique_ptr<UserEventReporterHelper> helper);
  // For prod. Uses the default implementation of UserEventReporterTestHelper.
  UserAddedRemovedReporter();

  UserAddedRemovedReporter(const UserAddedRemovedReporter& other) = delete;
  UserAddedRemovedReporter& operator=(const UserAddedRemovedReporter& other) =
      delete;

  ~UserAddedRemovedReporter() override;

  // Check the ChromeUserManager removed user cache for users removed before
  // the reporter is created.
  void ProcessRemoveUserCache();

  // ManagedSessionService::Observer overrides.
  // Check if login was to a new account.
  void OnLogin(Profile* profile) override;
  // Track that a user will be deleted. This should be done before users are
  // removed so that it can be checked if they are affiliated.
  void OnUserToBeRemoved(const AccountId& account_id) override;
  // Report that a user has been removed.
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reason) override;

 private:
  std::unique_ptr<UserEventReporterHelper> helper_;

  // Maps a user's email to if they are affiliated. This is needed to determine
  // if their email may be reported.
  base::flat_map<AccountId, bool> users_to_be_deleted_;

  policy::ManagedSessionService managed_session_service_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_ADDED_REMOVED_USER_ADDED_REMOVED_REPORTER_H_
