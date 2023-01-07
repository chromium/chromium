// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"

namespace reporting {

ReportQueueManualTestContext::ReportQueueManualTestContext(
    base::TimeDelta frequency,
    uint64_t number_of_messages_to_enqueue,
    Destination destination,
    Priority priority,
    CompletionCallback completion_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
    : TaskRunnerContext<Status>(std::move(completion_cb),
                                sequenced_task_runner),
      frequency_(frequency),
      number_of_messages_to_enqueue_(number_of_messages_to_enqueue),
      destination_(destination),
      priority_(priority),
      report_queue_(std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(sequenced_task_runner))) {}

ReportQueueManualTestContext::~ReportQueueManualTestContext() = default;

void ReportQueueManualTestContext::SetBuildReportQueueCallbackForTests(
    BuildReportQueueCallback build_report_queue_cb) {
  *GetBuildReportQueueCallback() = std::move(build_report_queue_cb);
}

void ReportQueueManualTestContext::OnStart() {
  if (destination_ == UNDEFINED_DESTINATION) {
    Status invalid_destination = Status(
        error::INVALID_ARGUMENT, "Destination was UNDEFINED_DESTINATION");
    LOG(ERROR) << invalid_destination;
    Complete(invalid_destination);
    return;
  }

  if (priority_ == UNDEFINED_PRIORITY) {
    Status invalid_priority = Status(error::INVALID_ARGUMENT,
                                     "Destination was UNDEFINED_DESTINATION");
    LOG(ERROR) << invalid_priority;
    Complete(invalid_priority);
    return;
  }

  Schedule(&ReportQueueManualTestContext::BuildReportQueue,
           base::Unretained(this));
}

void ReportQueueManualTestContext::BuildReportQueue() {
  CheckOnValidSequence();
  ReportQueueConfiguration::PolicyCheckCallback policy_check_cb =
      base::BindRepeating([]() -> Status { return Status::StatusOK(); });
  auto config_result = ReportQueueConfiguration::Create(
      // using an empty DM token cause device DM tokens are appended by default
      // during event uploads
      /*dm_token=*/"", destination_, std::move(policy_check_cb));
  if (!config_result.ok()) {
    Complete(config_result.status());
    return;
  }

  if (*GetBuildReportQueueCallback()) {
    std::move(*GetBuildReportQueueCallback())
        .Run(
            std::move(config_result.ValueOrDie()),
            base::BindOnce(&ReportQueueManualTestContext::OnReportQueueResponse,
                           base::Unretained(this)));
    return;
  }

  auto report_queue_result = ReportQueueProvider::CreateSpeculativeQueue(
      std::move(config_result.ValueOrDie()));
  Schedule(&ReportQueueManualTestContext::OnReportQueueResponse,
           base::Unretained(this), std::move(report_queue_result));
}

void ReportQueueManualTestContext::OnReportQueueResponse(
    StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>
        report_queue_result) {
  if (!report_queue_result.ok()) {
    Complete(report_queue_result.status());
    return;
  }

  report_queue_ = std::move(report_queue_result.ValueOrDie());
  Schedule(&ReportQueueManualTestContext::ScheduleEnqueue,
           base::Unretained(this));
}

void ReportQueueManualTestContext::ScheduleEnqueue() {
  CheckOnValidSequence();
  if (number_of_messages_to_enqueue_ > 0u &&
      number_of_messages_to_enqueue_ == number_of_enqueued_messages_) {
    Complete(Status::StatusOK());
    return;
  }

  ScheduleAfter(frequency_, &ReportQueueManualTestContext::Enqueue,
                base::Unretained(this));
}

void ReportQueueManualTestContext::Enqueue() {
  CheckOnValidSequence();
  report_queue_->Enqueue(
      base::NumberToString(value_++), priority_,
      base::BindOnce(&ReportQueueManualTestContext::OnEnqueue,
                     base::Unretained(this)));
  number_of_enqueued_messages_++;
}

void ReportQueueManualTestContext::OnEnqueue(Status status) {
  if (!status.ok()) {
    Complete(status);
    return;
  }

  Schedule(&ReportQueueManualTestContext::ScheduleEnqueue,
           base::Unretained(this));
}

void ReportQueueManualTestContext::Complete(Status status) {
  Schedule(&ReportQueueManualTestContext::Response, base::Unretained(this),
           status);
}

// static
ReportQueueManualTestContext::BuildReportQueueCallback*
ReportQueueManualTestContext::GetBuildReportQueueCallback() {
  static base::NoDestructor<BuildReportQueueCallback> callback{
      BuildReportQueueCallback()};
  return callback.get();
}
}  // namespace reporting
