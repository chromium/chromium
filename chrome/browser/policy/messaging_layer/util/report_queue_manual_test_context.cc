// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"

#include "base/callback.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/task_runner_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

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
      priority_(priority) {}

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

  // The DMToken must be retrieved on the UI thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&ReportQueueManualTestContext::GetDmToken,
                                base::Unretained(this)));
}

void ReportQueueManualTestContext::GetDmToken() {
  const policy::DMToken dm_token =
      policy::GetDMToken(ProfileManager::GetPrimaryUserProfile());
  Schedule(&ReportQueueManualTestContext::OnDmTokenResponse,
           base::Unretained(this), dm_token);
}

void ReportQueueManualTestContext::OnDmTokenResponse(policy::DMToken dm_token) {
  CheckOnValidSequence();
  if (!dm_token.is_valid()) {
    Status invalid_dm_token =
        Status(error::FAILED_PRECONDITION, "Cannot retrieve a valid DMToken");
    LOG(ERROR) << invalid_dm_token;
    Complete(invalid_dm_token);
    return;
  }
  dm_token_ = dm_token;
  BuildReportQueue();
}

void ReportQueueManualTestContext::BuildReportQueue() {
  CheckOnValidSequence();
  ReportQueueConfiguration::PolicyCheckCallback policy_check_cb =
      base::BindRepeating([]() -> Status { return Status::StatusOK(); });
  auto config_result = reporting::ReportQueueConfiguration::Create(
      dm_token_.value(), destination_, std::move(policy_check_cb));
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

  reporting::ReportQueueProvider::CreateQueue(
      std::move(config_result.ValueOrDie()),
      base::BindOnce(&ReportQueueManualTestContext::OnReportQueueResponse,
                     base::Unretained(this)));
}

void ReportQueueManualTestContext::OnReportQueueResponse(
    StatusOr<std::unique_ptr<ReportQueue>> report_queue_result) {
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
