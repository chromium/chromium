// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_

#include "ash/components/login/auth/auth_status_consumer.h"
#include "base/containers/queue.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/login_logout_event.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_provider.h"

namespace reporting {

class UserEventReporterHelper;

}  // namespace reporting

namespace ash {
namespace reporting {

class LoginLogoutReporter : public policy::ManagedSessionService::Observer {
 public:
  class Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    Delegate(const Delegate&& other) = delete;
    Delegate& operator=(const Delegate&& other) = delete;

    virtual ~Delegate() = default;

    // Get account id used in last login attempt, return empty account id if
    // unable to get it.
    virtual AccountId GetLastLoginAttemptAccountId() const;
  };

  LoginLogoutReporter(const LoginLogoutReporter& other) = delete;
  LoginLogoutReporter& operator=(const LoginLogoutReporter& other) = delete;
  LoginLogoutReporter(const LoginLogoutReporter&& other) = delete;
  LoginLogoutReporter& operator=(const LoginLogoutReporter&& other) = delete;

  ~LoginLogoutReporter() override;

  static std::unique_ptr<LoginLogoutReporter> Create(
      policy::ManagedSessionService* managed_session_service);

  static std::unique_ptr<LoginLogoutReporter> CreateForTest(
      std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
      std::unique_ptr<Delegate> delegate);

  // Report user device failed login attempt.
  void OnLoginFailure(const AuthFailure& error) override;

  // Report user device successful login.
  void OnLogin(Profile* profile) override;

  // Report user device logout.
  void OnSessionTerminationStarted(const user_manager::User* user) override;

 private:
  LoginLogoutReporter(
      std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper,
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  void MaybeReportEvent(LoginLogoutRecord record, const AccountId& account_id);

  std::unique_ptr<::reporting::UserEventReporterHelper> reporter_helper_;

  std::unique_ptr<Delegate> delegate_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};
};

}  // namespace reporting
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
