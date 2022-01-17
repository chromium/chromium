// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"

namespace reporting {

class UserEventReporterHelper {
 public:
  explicit UserEventReporterHelper(Destination destination,
                                   EventType event_type = EventType::kDevice);
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
  // Must be called on UI task runner (returned by valid_task_runner() below).
  virtual bool ShouldReportUser(const std::string& user_email) const;

  // Returns whether the provided reporting policy is set.
  // Must be called on UI task runner (returned by valid_task_runner() below).
  virtual bool ReportingEnabled(const std::string& policy_path) const;

  // Reports an event to the queue.
  // Thread safe, can be called on any thread.
  virtual void ReportEvent(const google::protobuf::MessageLite* record,
                           Priority priority);

  virtual bool IsCurrentUserNew() const;

  // Returns the only valid seq task runner for calls to ShouldReportUser and
  // ReportingEnabled.
  static scoped_refptr<base::SequencedTaskRunner> valid_task_runner();

 private:
  const std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_
