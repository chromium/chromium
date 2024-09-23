// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace google {
namespace protobuf {

class MessageLite;

}  // namespace protobuf
}  // namespace google

namespace reporting {

class Status;

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

  // DEPRECATED: please use ReportingUserTracker::ShouldReport() by passing
  // its instance from DeviceCloudPolicyManagerAsh.
  // TODO(b/267685577): Remove this.
  virtual bool ShouldReportUser(const std::string& user_email) const;

  // Returns whether the provided reporting policy is set.
  // Must be called on UI task runner (returned by valid_task_runner() below).
  virtual bool ReportingEnabled(const std::string& policy_path) const;

  // Returns whether the primary account is a kiosk.
  // Usually called if |ShouldReportUser| returned false.
  // Must be called on UI task runner (returned by valid_task_runner() below).
  virtual bool IsKioskUser() const;

  // Reports an event to the queue.
  // Thread safe, can be called on any thread.
  virtual void ReportEvent(
      std::unique_ptr<const google::protobuf::MessageLite> record,
      Priority priority,
      ReportQueue::EnqueueCallback enqueue_cb =
          base::BindOnce(&UserEventReporterHelper::OnEnqueueDefault));

  virtual bool IsCurrentUserNew() const;

  virtual std::string GetDeviceDmToken() const;

  // Generates a device-specific user ID for managed devices.
  // Returns a unique ID based on the user's email if the device is managed.
  // Otherwise, returns an empty string.
  virtual std::string GetUniqueUserIdForThisDevice(
      std::string_view user_email) const;

  // Returns the only valid seq task runner for calls to ShouldReportUser and
  // ReportingEnabled.
  static scoped_refptr<base::SequencedTaskRunner> valid_task_runner();

 private:
  static void OnEnqueueDefault(Status status);

  const std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_H_
