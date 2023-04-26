// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_CORE_REPORTING_USER_TRACKER_H_
#define CHROME_BROWSER_ASH_POLICY_CORE_REPORTING_USER_TRACKER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

class AccountId;
class PrefService;
class PrefRegistrySimple;

namespace policy {

// Keeps maintaining the list of users to be reported.
// This maintains persistent data in the given |local_state|.
class ReportingUserTracker : public user_manager::UserManager::Observer {
 public:
  explicit ReportingUserTracker(user_manager::UserManager* user_manager);
  ReportingUserTracker(const ReportingUserTracker&) = delete;
  ReportingUserTracker& operator=(const ReportingUserTracker&) = delete;
  ~ReportingUserTracker() override;

  // Registers prefs used by this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Return whether the given user should be reported
  // Returns whether the user email can be included in the report. By default,
  // only affiliated user emails are included. Function can accept
  // canonicalized and non canonicalized user_email.
  // Must be called on UI task runner.
  // See also policy::DeviceStatusCollector.
  bool ShouldReportUser(const std::string& user_email) const;

  // user_manager::UserManager::Observer:
  void OnUserAffiliationUpdated(const user_manager::User& user) override;
  void OnUserRemoved(const AccountId& account_id,
                     user_manager::UserRemovalReason reasonx) override;

 private:
  // Adds user to the list of the users who should be reported.
  void AddReportingUser(const AccountId& account_id);

  // Removes user from the list of the users who should be reported.
  void RemoveReportingUser(const AccountId& account_id);

  const raw_ptr<PrefService> local_state_;
  base::ScopedObservation<user_manager::UserManager,
                          user_manager::UserManager::Observer>
      observation_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_CORE_REPORTING_USER_TRACKER_H_
