// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_REPORTING_USER_EVENT_REPORTER_BASE_H_
#define CHROME_BROWSER_ASH_REPORTING_USER_EVENT_REPORTER_BASE_H_

#include <memory>

#include "base/strings/string_piece_forward.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue_provider.h"

namespace reporting {

class UserEventReporterBase {
 public:
  explicit UserEventReporterBase(::reporting::Destination destination);
  UserEventReporterBase(const UserEventReporterBase& other) = delete;
  UserEventReporterBase& operator=(const UserEventReporterBase& other) = delete;
  virtual ~UserEventReporterBase();

 protected:
  // Allows test classes to pass in user defined report queues for testing.
  explicit UserEventReporterBase(
      ::reporting::Destination destination,
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue);

  // Returns whether the user email can be included in the report. By default,
  // only affiliated user emails are included. Function can accept
  // canonicalized and non canonicalized user_email.
  virtual bool ShouldReportUser(const std::string& user_email) const;

  // Returns whether the provided reporting policy is set.
  virtual bool ReportingEnabled(const std::string& policy_path) const;

  // Reports an event to the queue.
  virtual void ReportEvent(base::StringPiece record,
                           ::reporting::Priority priority);

  // Returns the device DM token.
  static policy::DMToken GetDMToken();

  std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
      report_queue_;

  const ::reporting::Destination destination_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_BASE_H_
