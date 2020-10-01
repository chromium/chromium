// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/meet_device_telemetry_report_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/values.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/policy/messaging_layer/upload/app_install_report_handler.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"

namespace reporting {

namespace {

// Common Key names used when building the dictionary to pass to the Chrome
// Reporting API.
constexpr char kDestination[] = "destination";
constexpr char kDmToken[] = "dmToken";
constexpr char kTimestampUs[] = "timestampUs";
constexpr char kData[] = "data";

base::Value ConvertRecordProtoToValue(const Record& record,
                                      const base::Value& context) {
  base::Value record_fields{base::Value::Type::DICTIONARY};
  if (record.has_destination()) {
    record_fields.SetIntKey(kDestination, record.destination());
  }
  if (!record.dm_token().empty()) {
    record_fields.SetStringKey(kDmToken, record.dm_token());
  }
  if (record.has_timestamp_us()) {
    // Do not convert into RFC3339 format - we need to keep microseconds.
    // 64-bit ints aren't supported by JSON - must be stored as strings
    std::ostringstream str;
    str << record.timestamp_us();
    record_fields.SetStringKey(kTimestampUs, str.str());
  }
  if (record.has_data()) {  // No data indicates gap, empty data is still data.
    record_fields.SetStringKey(kData, record.data());
  }
  base::Value records_list{base::Value::Type::LIST};
  records_list.Append(std::move(record_fields));
  return records_list;
}

}  // namespace

MeetDeviceTelemetryReportHandler::MeetDeviceTelemetryReportHandler(
    Profile* profile,
    policy::CloudPolicyClient* client)
    : AppInstallReportHandler(client), profile_(profile) {}

MeetDeviceTelemetryReportHandler::~MeetDeviceTelemetryReportHandler() = default;

Status MeetDeviceTelemetryReportHandler::ValidateRecord(
    const Record& record) const {
  RETURN_IF_ERROR(
      ValidateDestination(record, Destination::MEET_DEVICE_TELEMETRY));
  if (!record.has_data()) {
    return Status(error::INVALID_ARGUMENT, "No 'data' in the Record");
  }
  return Status::StatusOK();
}

StatusOr<base::Value> MeetDeviceTelemetryReportHandler::ConvertRecord(
    const Record& record) const {
  base::Value context = reporting::GetContext(profile_);
  base::Value event_list = ConvertRecordProtoToValue(record, context);
  return policy::RealtimeReportingJobConfiguration::BuildReport(
      std::move(event_list), std::move(context));
}

}  // namespace reporting
