// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOCK_UNLOCK_REPORTER_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOCK_UNLOCK_REPORTER_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/lock_unlock_event.pb.h"

namespace reporting {
class UserEventReporterHelper;
}

namespace ash {
namespace reporting {

// This class is used to report events when an affiliated user locks or unlocks
// a managed device. It will report this when the policy
// kReportDeviceLoginLogout is true.
class LockUnlockReporter : public policy::ManagedSessionService::Observer {
 public:
  LockUnlockReporter(const LockUnlockReporter& other) = delete;
  LockUnlockReporter& operator=(const LockUnlockReporter& other) = delete;

  ~LockUnlockReporter() override;

  // For prod. Uses the default implementation of UserEventReporterHelper.
  static std::unique_ptr<LockUnlockReporter> Create(
      policy::ManagedSessionService* managed_session_service);

  // For use in testing only. Allows user to pass in a test helper.
  static std::unique_ptr<LockUnlockReporter> CreateForTest(
      std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
      policy::ManagedSessionService* managed_session_service,
      base::Clock* clock = base::DefaultClock::GetInstance());

  // policy::ManagedSessionService::Observer
  void OnLocked() override;

  void OnUnlockAttempt(const bool success,
                       const session_manager::UnlockType unlock_type) override;

 private:
  LockUnlockReporter(
      std::unique_ptr<::reporting::UserEventReporterHelper> helper,
      policy::ManagedSessionService* managed_session_service,
      base::Clock* clock = base::DefaultClock::GetInstance());

  void MaybeReportEvent(LockUnlockRecord record);

  raw_ptr<base::Clock> const clock_;

  std::unique_ptr<::reporting::UserEventReporterHelper> helper_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};
};
}  // namespace reporting
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOCK_UNLOCK_REPORTER_H_
