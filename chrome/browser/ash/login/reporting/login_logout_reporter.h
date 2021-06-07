// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
#define CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_

#include "chrome/browser/ash/login/reporting/login_logout_record.pb.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_provider.h"

namespace chromeos {
namespace reporting {

class LoginLogoutReporter {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;

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
  };

  // Default parameter value used in production, and set in testing.
  explicit LoginLogoutReporter(
      std::unique_ptr<Delegate> delegate = std::make_unique<Delegate>());

  LoginLogoutReporter(const LoginLogoutReporter& other) = delete;
  LoginLogoutReporter& operator=(const LoginLogoutReporter& other) = delete;

  virtual ~LoginLogoutReporter();

  // Report user device login if allowed by policy. Function can accept
  // canonicalized and non canonicalized user_email.
  void MaybeReportLogin(base::StringPiece user_email) const;

  // Report user device logout if allowed by policy. Function can accept
  // canonicalized and non canonicalized user_email.
  void MaybeReportLogout(base::StringPiece user_email) const;

 private:
  std::unique_ptr<Delegate> delegate_;

  static void OnReportQueueCreated(
      LoginLogoutRecord record,
      ::reporting::StatusOr<std::unique_ptr<::reporting::ReportQueue>>
          report_queue_result);

  static void OnRecordEnqueued(::reporting::ReportQueue* report_queue,
                               ::reporting::Status status);

  void MaybeReportEvent(base::StringPiece user_email,
                        LoginLogoutRecord record) const;
};
}  // namespace reporting
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_REPORTING_LOGIN_LOGOUT_REPORTER_H_
