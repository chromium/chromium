// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORT_QUEUE_MANUAL_TEST_CONTEXT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORT_QUEUE_MANUAL_TEST_CONTEXT_H_

#include "base/callback.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {

// This is a test fixture for manually testing uploading of events. Should only
// be used when debugging or testing. It will enqueue an incrementing uint64
// every |frequency| for the provided destination. All messages will be sent as
// FAST_BATCH.
//
// This context can be used in the following way:
//   Start<ReportQueueManualTestContext>(base::TimeDelta::FromSeconds(1),
//                             /*number_of_messages_to_enqueue=*/10,
//                             UPLOAD_EVENTS,
//                             FAST_BATCH,
//                             base::BindOnce(
//                               [](Status status) {
//                                 LOG(INFO) << status;
//                              }),
//                              base::ThreadPool::CreateSequencedTaskRunner(
//                                base::TaskTraits()));
// As configured this context will create a ReportQueue and upload an event
// to the FAST_BATCH priority every second for 10 seconds.
class ReportQueueManualTestContext : public TaskRunnerContext<Status> {
 public:
  using CompletionCallback = base::OnceCallback<void(Status)>;
  using BuildReportQueueCallback = base::OnceCallback<void(
      std::unique_ptr<ReportQueueConfiguration>,
      base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>)>;

  ReportQueueManualTestContext(
      base::TimeDelta frequency,
      uint64_t number_of_messages_to_enqueue,
      Destination destination,
      Priority priorty,
      CompletionCallback completion_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  // Sets the callback for building the report queue. Should be called before
  // calling ReportQueueManualTestContext().
  static void SetBuildReportQueueCallbackForTests(
      BuildReportQueueCallback build_report_queue_cb);

 private:
  ~ReportQueueManualTestContext() override;

  void OnStart() override;

  void GetDmToken();
  void OnDmTokenResponse(policy::DMToken dm_token);
  void BuildReportQueue();
  void OnReportQueueResponse(
      StatusOr<std::unique_ptr<ReportQueue>> report_queue_result);
  void ScheduleEnqueue();
  void Enqueue();
  void OnEnqueue(Status status);

  void Complete(Status status);

  static BuildReportQueueCallback* GetBuildReportQueueCallback();

  // Frequency at which messages should be enqueued.
  const base::TimeDelta frequency_;

  // Total number of messages that should be enqueued.
  // If set to 0, messages will be sent until the user ends the session or
  // ReportQueue::Enqueue sends back a not-OK error.
  const uint64_t number_of_messages_to_enqueue_;

  const Destination destination_;
  const Priority priority_;

  // Counter for the number of messages sent. Should only be accessed while on
  // sequence.
  uint64_t number_of_enqueued_messages_{0u};

  // Counter for the current value being enqueued. Should only be accessed while
  // on sequence.
  uint64_t value_{0u};

  policy::DMToken dm_token_;
  std::unique_ptr<ReportQueue> report_queue_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORT_QUEUE_MANUAL_TEST_CONTEXT_H_
