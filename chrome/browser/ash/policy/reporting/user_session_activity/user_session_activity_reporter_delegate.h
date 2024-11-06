// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_SESSION_ACTIVITY_USER_SESSION_ACTIVITY_REPORTER_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_SESSION_ACTIVITY_USER_SESSION_ACTIVITY_REPORTER_DELEGATE_H_

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"
#include "chrome/browser/ash/policy/reporting/user_session_activity/user_session_activity_reporter.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/ash/power/ml/idle_event_notifier.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/user_session_activity.pb.h"

namespace reporting {

// Delegate class for `reporting::UserSessionActivityReporter`.
// This class holds the internal session activity state, reports the state, and
// queries the active/idle status of the device.
//
// This class is not thread safe - all functions should be called from the same
// sequenced task runner.
//
// The delegate does NOT persist state to disk, so if Chrome
// crashes in the middle of a session, data is lost.
class UserSessionActivityReporterDelegate
    : public reporting::UserSessionActivityReporter::Delegate {
 public:
  UserSessionActivityReporterDelegate(
      std::unique_ptr<reporting::UserEventReporterHelper> reporter_helper,
      std::unique_ptr<ash::power::ml::IdleEventNotifier> idle_event_notifier_);

  UserSessionActivityReporterDelegate(
      const UserSessionActivityReporterDelegate& other) = delete;
  UserSessionActivityReporterDelegate& operator=(
      const UserSessionActivityReporterDelegate& other) = delete;
  ~UserSessionActivityReporterDelegate() override;

  // Gets the device active/idle status.
  ash::power::ml::IdleEventNotifier::ActivityData QueryIdleStatus()
      const override;

  // Returns true if the user has been recently active during the session. False
  // otherwise.
  bool IsUserActive(const ash::power::ml::IdleEventNotifier::ActivityData&
                        activity_data) const override;

  // Enqueues a record containing the current session activity to the encrypted
  // reporting pipeline.
  void ReportSessionActivity() override;

  // Adds an active or idle state to the current session activity state.
  void AddActiveIdleState(bool is_user_active,
                          const user_manager::User* user) override;

  // Sets the session start field in the session activity state.
  void SetSessionStartEvent(reporting::SessionStartEvent::Reason reason,
                            const user_manager::User* user) override;

  // Sets the session end field in the session activity state.
  void SetSessionEndEvent(reporting::SessionEndEvent::Reason reason,
                          const user_manager::User* user) override;

  // Sets the user in a UserSessionActivityRecord. Does nothing if `record`
  // already has a user.
  void SetUser(UserSessionActivityRecord* record,
               const user_manager::User* user);

 private:
  // Clears the session activity state and resets the internal state of the idle
  // event notifier.
  void Reset();

  SEQUENCE_CHECKER(sequence_checker_);

  UserSessionActivityRecord session_activity_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<UserEventReporterHelper> reporter_helper_
      GUARDED_BY_CONTEXT(sequence_checker_);

  std::unique_ptr<ash::power::ml::IdleEventNotifier> idle_event_notifier_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_SESSION_ACTIVITY_USER_SESSION_ACTIVITY_REPORTER_DELEGATE_H_
