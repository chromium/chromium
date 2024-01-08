// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/auth_status_consumer.h"

class PrefRegistrySimple;

namespace reporting {

class UserEventReporterHelper;

}  // namespace reporting

namespace ash {

class AuthFailure;

namespace reporting {

class LoginLogoutReporter : public policy::ManagedSessionService::Observer {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;

    virtual ~Delegate() = default;

    // Get account id used in last login attempt, return empty account id if
    // unable to get it.
    virtual AccountId GetLastLoginAttemptAccountId() const;
  };

  LoginLogoutReporter(const LoginLogoutReporter& other) = delete;
  LoginLogoutReporter& operator=(const LoginLogoutReporter& other) = delete;

  ~LoginLogoutReporter() override;

  static std::unique_ptr<LoginLogoutReporter> Create(
      policy::ManagedSessionService* managed_session_service);

  static std::unique_ptr<LoginLogoutReporter> CreateForTest(
      std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service,
      base::Clock* clock = base::DefaultClock::GetInstance());

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Report user device failed login attempt.
  void OnLoginFailure(const AuthFailure& error) override;

  // Report user device successful login.
  void OnLogin(Profile* profile) override;

  // Report user device logout.
  void OnSessionTerminationStarted(const user_manager::User* user) override;

  void OnKioskLoginFailure() override;

 private:
  LoginLogoutReporter(
      std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service,
      base::Clock* clock = base::DefaultClock::GetInstance());

  void MaybeReportEvent(LoginLogoutRecord record, const AccountId& account_id);

  void MaybeReportKioskLoginFailure();

  std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper_;

  std::unique_ptr<Delegate> delegate_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  const raw_ptr<base::Clock> clock_;

  // To be able to access |kEnableKioskAndGuestLoginLogoutReporting| in tests.
  friend class LoginLogoutTestHelper;
};

}  // namespace reporting
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
