// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORT_QUEUE_MANUAL_TEST_CONTEXT_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORT_QUEUE_MANUAL_TEST_CONTEXT_H_

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {

// This is a test fixture for manually testing uploading of device events.
// Should only be used when debugging or testing. It will enqueue an
// incrementing uint64 every `period` for the provided destination. All
// messages will be sent as FAST_BATCH.
//
// This context can be used in the following way:
//   Start<ReportQueueManualTestContext>(
//      /*period=*/base::Seconds(1),
//      /*number_of_messages_to_enqueue=*/10,
//      UPLOAD_EVENTS,
//      FAST_BATCH,
//      EventType::kUser,
//      base::BindOnce([](Status status) { LOG(INFO) << status; }),
//      base::ThreadPool::CreateSequencedTaskRunner());
// As configured this context will create a ReportQueue and upload an event
// to the FAST_BATCH priority every second for 10 seconds.
//
// This context does not support user events today, but support for user events
// may be added later, if necessary. Note however, that for user events
// respective DM token needs to be supplied.
class ReportQueueManualTestContext : public TaskRunnerContext<Status> {
 public:
  using CompletionCallback = base::OnceCallback<void(Status)>;
  using BuildReportQueueCallback = base::OnceCallback<
      StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>(
          std::unique_ptr<ReportQueueConfiguration>)>;

  ReportQueueManualTestContext(
      base::TimeDelta period,
      uint64_t number_of_messages_to_enqueue,
      Destination destination,
      Priority priorty,
      EventType event_type,
      CompletionCallback completion_cb,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
      BuildReportQueueCallback queue_builder =  // can be replaced in test
      base::BindOnce(&ReportQueueProvider::CreateSpeculativeQueue));

 private:
  ~ReportQueueManualTestContext() override;

  void OnStart() override;

  void NextEnqueue();
  void Enqueue();
  void OnEnqueued(Status status);

  void Complete(Status status);

  // Period at which messages should be enqueued.
  const base::TimeDelta period_;

  // Total number of messages that should be enqueued.
  // If set to 0, messages will be sent until the user ends the session or
  // ReportQueue::Enqueue sends back a not-OK error.
  const uint64_t number_of_messages_to_enqueue_;

  const Destination destination_;
  const Priority priority_;
  const EventType event_type_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Counter for the number of messages sent.
  uint64_t number_of_enqueued_messages_ GUARDED_BY_CONTEXT(sequence_checker_) =
      0u;

  // Counter for the current value being enqueued.
  uint64_t value_ GUARDED_BY_CONTEXT(sequence_checker_) = 0u;

  // Callback to build report queue.
  // `ReportQueueProvider::CreateSpeculativeQueue` by default.
  BuildReportQueueCallback queue_builder_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Resulting report queue.
  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UTIL_REPORT_QUEUE_MANUAL_TEST_CONTEXT_H_
