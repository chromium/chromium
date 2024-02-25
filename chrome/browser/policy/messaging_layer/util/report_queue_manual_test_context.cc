// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
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
    base::TimeDelta period,
    uint64_t number_of_messages_to_enqueue,
    Destination destination,
    Priority priority,
    EventType event_type,
    CompletionCallback completion_cb,
    scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
    BuildReportQueueCallback queue_builder)
    : TaskRunnerContext<Status>(std::move(completion_cb),
                                sequenced_task_runner),
      period_(period),
      number_of_messages_to_enqueue_(number_of_messages_to_enqueue),
      destination_(destination),
      priority_(priority),
      event_type_(event_type),
      queue_builder_(std::move(queue_builder)),
      report_queue_(std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(sequenced_task_runner))) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ReportQueueManualTestContext::~ReportQueueManualTestContext() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ReportQueueManualTestContext::OnStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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

  auto config_result =
      ReportQueueConfiguration::Create(
          {.event_type = event_type_, .destination = destination_})
          .Build();
  if (!config_result.has_value()) {
    Complete(config_result.error());
    return;
  }

  // Build queue by configuration.
  CHECK(queue_builder_) << "Can be only called once";
  auto report_queue_result =
      std::move(queue_builder_).Run(std::move(config_result.value()));
  if (!report_queue_result.has_value()) {
    Complete(report_queue_result.error());
    return;
  }

  report_queue_ = std::move(report_queue_result.value());
  NextEnqueue();
}

void ReportQueueManualTestContext::NextEnqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (number_of_enqueued_messages_ >= number_of_messages_to_enqueue_) {
    Complete(Status::StatusOK());
    return;
  }

  ScheduleAfter(period_, &ReportQueueManualTestContext::Enqueue,
                base::Unretained(this));
}

void ReportQueueManualTestContext::Enqueue() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  report_queue_->Enqueue(
      base::NumberToString(value_++), priority_,
      base::BindOnce(&ReportQueueManualTestContext::OnEnqueued,
                     base::Unretained(this)));
  number_of_enqueued_messages_++;
}

void ReportQueueManualTestContext::OnEnqueued(Status status) {
  if (!status.ok()) {
    Complete(status);
    return;
  }

  Schedule(&ReportQueueManualTestContext::NextEnqueue, base::Unretained(this));
}

void ReportQueueManualTestContext::Complete(Status status) {
  Schedule(&ReportQueueManualTestContext::Response, base::Unretained(this),
           status);
}
}  // namespace reporting
