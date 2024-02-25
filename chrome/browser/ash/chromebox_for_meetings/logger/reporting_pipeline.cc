// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/chromebox_for_meetings/logger/reporting_pipeline.h"

#include <cstdint>
#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"
#include "components/reporting/proto/synced/record_constants.pb.h"

namespace ash::cfm {

namespace {

// TODO(https://crbug.com/1403174): Remove when namespace of mojoms for CfM are
// migarted to ash.
namespace mojom = ::chromeos::cfm::mojom;

::reporting::Priority ToReportingPriority(mojom::EnqueuePriority priority) {
  switch (priority) {
    case mojom::EnqueuePriority::kHigh:
      return ::reporting::Priority::FAST_BATCH;
    case mojom::EnqueuePriority::kMedium:
      return ::reporting::Priority::SLOW_BATCH;
    case mojom::EnqueuePriority::kLow:
      return ::reporting::Priority::BACKGROUND_BATCH;
  }
}

mojom::LoggerStatusPtr LoggerUninitializedStatus() {
  return mojom::LoggerStatus::New(mojom::LoggerErrorCode::kUnavailable,
                                  "Meet logger service not yet initialised.");
}

::reporting::Status LogPolicyDisabled() {
  return ::reporting::Status(reporting::error::UNAUTHENTICATED,
                             "System log upload policy not enabled");
}

constexpr auto kHandlerDestination =
    ::reporting::Destination::MEET_DEVICE_TELEMETRY;

}  // namespace

ReportingPipeline::ReportingPipeline(
    UpdateStatusCallback update_status_callback)
    : update_status_callback_(std::move(update_status_callback)),
      task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ReportingPipeline::~ReportingPipeline() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Reset();
}

void ReportingPipeline::Init() {
  CHECK(DeviceSettingsService::IsInitialized());
  DeviceSettingsService::Get()->AddObserver(this);
  // Device settings update may not be triggered in some cases
  DeviceSettingsUpdated();
}

void ReportingPipeline::Reset() {
  DeviceSettingsService::Get()->RemoveObserver(this);
  dm_token_.clear();
  update_status_callback_.Run(mojom::LoggerState::kUninitialized);
}

void ReportingPipeline::Enqueue(const std::string& record,
                                mojom::EnqueuePriority priority,
                                CfmLoggerService::EnqueueCallback callback) {
  if (!report_queue_) {
    LOG(ERROR) << "Report Queue has not been initialised";
    std::move(callback).Run(LoggerUninitializedStatus());
    return;
  }

  report_queue_->Enqueue(
      std::move(record), ToReportingPriority(priority),
      base::BindOnce(
          [](CfmLoggerService::EnqueueCallback callback,
             reporting::Status status) {
            auto message = status.error_message();
            auto code =
                static_cast<mojom::LoggerErrorCode>(status.error_code());

            if (!mojom::IsKnownEnumValue(code))
              code = mojom::LoggerErrorCode::kUnknown;

            std::move(callback).Run(mojom::LoggerStatus::New(
                std::move(code), std::string(message.begin(), message.end())));
          },
          std::move(callback)));
}

void ReportingPipeline::DeviceSettingsUpdated() {
  auto* policy_data = DeviceSettingsService::Get()->policy_data();

  if (!policy_data || !policy_data->has_request_token() ||
      policy_data->request_token().empty()) {
    LOG(ERROR) << "Unable to obtain information from Policy Data";
    return;
  }

  // Post to primary task runner
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ReportingPipeline::UpdateToken,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        policy_data->request_token()));
}

void ReportingPipeline::UpdateToken(std::string request_token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (dm_token_ == request_token) {
    VLOG(4) << "DMToken received is already being used.";
    return;
  }

  dm_token_ = request_token;

  auto config_result = reporting::ReportQueueConfiguration::Create(
      dm_token_, kHandlerDestination,
      base::BindRepeating(&ReportingPipeline::CheckPolicy,
                          base::Unretained(this)));

  if (!config_result.has_value()) {
    LOG(ERROR) << "Report Client Configuration failed with error message: "
               << config_result.error();
    // Reset DMToken to allow future attempts at configuring the report queue.
    // TODO(b/175156039): Attempt to create a new configuration again.
    dm_token_.clear();
    return;
  }

  auto queue_callback = base::BindPostTask(
      task_runner_, base::BindOnce(&ReportingPipeline::OnReportQueueUpdated,
                                   weak_ptr_factory_.GetWeakPtr()));

  // Asynchronously create ReportingQueue.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<reporting::ReportQueueConfiguration> config,
             reporting::ReportQueueProvider::CreateReportQueueCallback
                 queue_callback) {
            reporting::ReportQueueProvider::CreateQueue(
                std::move(config), std::move(queue_callback));
          },
          std::move(config_result).value(), std::move(queue_callback)));
}

::reporting::Status ReportingPipeline::CheckPolicy() const {
  auto* chrome_device_settings =
      DeviceSettingsService::Get()->device_settings();
  bool policy_enabled = false;

  if (chrome_device_settings &&
      chrome_device_settings->has_device_log_upload_settings()) {
    auto device_upload_settings =
        chrome_device_settings->device_log_upload_settings();
    policy_enabled = device_upload_settings.system_log_upload_enabled();
  }

  return policy_enabled ? ::reporting::Status::StatusOK() : LogPolicyDisabled();
}

void ReportingPipeline::OnReportQueueUpdated(
    reporting::ReportQueueProvider::CreateReportQueueResponse
        report_queue_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!report_queue_result.has_value()) {
    LOG(ERROR) << "Report Queue creation failed with error message: "
               << report_queue_result.error();
    // Reset DMToken to allow future attempts at creating a report queue.
    // TODO(b/175156039): Attempt to create a new queue again.
    dm_token_.clear();
    return;
  }

  report_queue_ = std::move(report_queue_result.value());

  update_status_callback_.Run(mojom::LoggerState::kReadyForRequests);

  VLOG(3) << "Report Queue successfully created.";
}

void ReportingPipeline::OnDeviceSettingsServiceShutdown() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&ReportingPipeline::Reset,
                                        weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash::cfm
