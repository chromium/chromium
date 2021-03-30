// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_LOGGER_REPORTING_PIPELINE_H_
#define CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_LOGGER_REPORTING_PIPELINE_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/chromeos/chromebox_for_meetings/logger/cfm_logger_service.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_provider.h"

namespace chromeos {
namespace cfm {

// Implementation of the CfmLoggerService::Delegate usign the chrome encrypted
// reporting pipeline.
class ReportingPipeline : public CfmLoggerService::Delegate,
                          public ash::DeviceSettingsService::Observer {
 public:
  // Args: mojom::MeetDevicesLogger: The current enabled state of the service.
  using UpdateStatusCallback =
      base::RepeatingCallback<void(mojom::LoggerState)>;

  explicit ReportingPipeline(UpdateStatusCallback update_status_callback);
  ReportingPipeline(const ReportingPipeline&) = delete;
  ReportingPipeline& operator=(const ReportingPipeline&) = delete;
  ~ReportingPipeline() override;

  // CfmLoggerService::Delegate implementation
  void Init() override;
  void Reset() override;
  void Enqueue(const std::string& record,
               mojom::EnqueuePriority priority,
               CfmLoggerService::EnqueueCallback callback) override;

 protected:
  // ::ash::DeviceSettingsService::Observer impl
  void DeviceSettingsUpdated() override;
  void OnDeviceSettingsServiceShutdown() override;

 private:
  void UpdateToken(std::string request_token);
  void OnReportQueueUpdated(
      reporting::ReportQueueProvider::CreateReportQueueResponse
          report_queue_result);

  UpdateStatusCallback update_status_callback_;
  std::unique_ptr<reporting::ReportQueue> report_queue_;
  std::string dm_token_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<ReportingPipeline> weak_ptr_factory_{this};
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHROMEBOX_FOR_MEETINGS_LOGGER_REPORTING_PIPELINE_H_
