// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_MEET_DEVICE_TELEMETRY_REPORT_HANDLER_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_MEET_DEVICE_TELEMETRY_REPORT_HANDLER_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/app_install_report_handler.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"

namespace reporting {

// |MeetDeviceTelemetryReportHandler| wraps |AppInstallReportHandler| to send
// MeetDeviceTelemetry data to DM server with MEET_DEVICE_TELEMETRY destination
// using |CloudPolicyClient|. Since |CloudPolicyClient| will cancel any in
// progress reports if a new report is added, |AppInstallReportHandler| ensures
// that only one report is ever processed at one time by forming a queue.
// Exists only on ChromeOS.
class MeetDeviceTelemetryReportHandler : public AppInstallReportHandler {
 public:
  // The client uses a boolean value for status, where true indicates success
  // and false indicates failure.
  using ClientCallback = AppInstallReportHandler::ClientCallback;

  MeetDeviceTelemetryReportHandler(Profile* profile,
                                   policy::CloudPolicyClient* client);
  ~MeetDeviceTelemetryReportHandler() override;

 private:
  Status ValidateRecord(const Record& record) const override;
  StatusOr<base::Value> ConvertRecord(const Record& record) const override;

  Profile* const profile_;
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_MEET_DEVICE_TELEMETRY_REPORT_HANDLER_H_
