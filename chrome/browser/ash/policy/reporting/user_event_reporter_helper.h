// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"

namespace reporting {

class UserEventReporterHelper {
 public:
  explicit UserEventReporterHelper(Destination destination);
  // Allows test classes to pass in user defined report queues for testing.
  explicit UserEventReporterHelper(
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue);
  UserEventReporterHelper(const UserEventReporterHelper& other) = delete;
  UserEventReporterHelper& operator=(const UserEventReporterHelper& other) =
      delete;
  virtual ~UserEventReporterHelper();

  // Returns whether the user email can be included in the report. By default,
  // only affiliated user emails are included. Function can accept
  // canonicalized and non canonicalized user_email.
  virtual bool ShouldReportUser(const std::string& user_email) const;

  // Returns whether the provided reporting policy is set.
  virtual bool ReportingEnabled(const std::string& policy_path) const;

  // Reports an event to the queue.
  virtual void ReportEvent(const google::protobuf::MessageLite* record,
                           Priority priority);

  virtual bool IsCurrentUserNew() const;

 private:
  const std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_
