// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_

#include "base/containers/queue.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/login/reporting/login_logout_record.pb.h"
#include "chrome/browser/ash/policy/status_collector/managed_session_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_provider.h"

namespace chromeos {
namespace reporting {

class LoginLogoutReporter : public policy::ManagedSessionService::Observer {
 public:
  class Delegate {
   public:
    Delegate();

    Delegate(const Delegate& other) = delete;
    Delegate& operator=(const Delegate& other) = delete;
    Delegate(const Delegate&& other) = delete;
    Delegate& operator=(const Delegate&& other) = delete;

    virtual ~Delegate();

    // Return whether the login/logout event reporting is allowed by policy.
    virtual bool ShouldReportEvent() const;

    // Return whether the user email can be included the login/logout report,
    // only affiliated user emails are included. Function can accept
    // canonicalized and non canonicalized user_email.
    virtual bool ShouldReportUser(base::StringPiece user_email) const;

    // Return the device DM token.
    virtual policy::DMToken GetDMToken() const;

    // Create a new reporting pipeline queue to be used for reporting the
    // login/logout event.
    virtual void CreateReportingQueue(
        std::unique_ptr<::reporting::ReportQueueConfiguration> config,
        ::reporting::ReportQueueProvider::CreateReportQueueCallback
            create_queue_cb);

    // Get account id used in last login attempt, return empty account id if
    // unable to get it.
    virtual AccountId GetLastLoginAttemptAccountId() const;

   private:
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  };

  LoginLogoutReporter(const LoginLogoutReporter& other) = delete;
  LoginLogoutReporter& operator=(const LoginLogoutReporter& other) = delete;
  LoginLogoutReporter(const LoginLogoutReporter&& other) = delete;
  LoginLogoutReporter& operator=(const LoginLogoutReporter&& other) = delete;

  // Default parameter value used in production, and set in testing.
  explicit LoginLogoutReporter(
      std::unique_ptr<Delegate> delegate = std::make_unique<Delegate>());

  ~LoginLogoutReporter() override;

  // Report user device failed login attempt.
  void OnLoginFailure(const chromeos::AuthFailure& error) override;

  // Report user device successful login.
  void OnLogin(Profile* profile) override;

  // Report user device logout.
  void OnSessionTerminationStarted(const user_manager::User* user) override;

 private:
  static LoginFailureReason GetLoginFailureReasonForReport(
      const chromeos::AuthFailure& error);

  void OnReportQueueCreated(
      ::reporting::StatusOr<std::unique_ptr<::reporting::ReportQueue>>
          report_queue_result);

  void EnqueueRecord(const LoginLogoutRecord& record);

  static void OnRecordEnqueued(::reporting::Status status);

  void Init();

  void MaybeReportEvent(LoginLogoutRecord record,
                        base::StringPiece user_email,
                        bool is_guest);

  std::unique_ptr<Delegate> delegate_;

  std::unique_ptr<::reporting::ReportQueue> report_queue_;

  base::queue<LoginLogoutRecord> pending_events_;

  bool is_initialized_ = false;

  policy::ManagedSessionService managed_session_service_;

  base::ScopedObservation<policy::ManagedSessionService,
                          policy::ManagedSessionService::Observer>
      managed_session_observation_{this};

  base::WeakPtrFactory<LoginLogoutReporter> weak_ptr_factory_{this};
};
}  // namespace reporting
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
