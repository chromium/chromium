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

// Common Key names used when building the Event dictionary to pass to the
// Chrome Reporting API.
// When sent the event should look like this:
// {
//   "time": "2017-01-15T01:30:15.01Z", // Timestamp in RFC3339 format
//   "reportingRecordEvent": {
//     "destination": 3,
//     "dmToken": "abcdef1234",
//     "timestampUs"": 123456,
//     "data": "String of Data"
//   }
// }
constexpr char kTime[] = "time";
constexpr char kReportingRecordEvent[] = "reportingRecordEvent";

// Common key names used when builing the ReportingRecordEvent dictionary to
// pass to Chrome Reporting API. This is the dictionary indicated by
// |kReportingRecordEvent|. This dictionary is a direct 1:1 relation to the
// reporting::Record proto.
constexpr char kDestination[] = "destination";
constexpr char kDmToken[] = "dmToken";
constexpr char kTimestampUs[] = "timestampUs";
constexpr char kData[] = "data";

// Takes a reporting::Record and converts it to the |kReportingRecordEvent|
// dictionary.
base::Value ConvertRecordProtoToValue(const Record& record) {
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
  return record_fields;
}

// Generates a RFC3999 time string.
std::string GetTimeString(const base::Time& timestamp) {
  base::Time::Exploded time_exploded;
  timestamp.UTCExplode(&time_exploded);
  std::string time_str = base::StringPrintf(
      "%d-%02d-%02dT%02d:%02d:%02d.%03dZ", time_exploded.year,
      time_exploded.month, time_exploded.day_of_month, time_exploded.hour,
      time_exploded.minute, time_exploded.second, time_exploded.millisecond);
  return time_str;
}

// Creates a list of Event dictionaries from a Record. (Note: Currently takes
// one Record and makes a list with one Event).
base::Value ConvertRecordProtoToEventList(const Record& record) {
  // Create the Event Dictionary
  base::Value event{base::Value::Type::DICTIONARY};

  // Set the |kReportingRecordEvent| key to the Record dictionary.
  event.SetKey(kReportingRecordEvent, ConvertRecordProtoToValue(record));

  // Build the timestamp and set the |kTime| to the RFC3999 version.
  base::Time timestamp =
      base::Time::UnixEpoch() +
      base::TimeDelta::FromMicroseconds(record.timestamp_us());
  event.SetStringKey(kTime, GetTimeString(timestamp));

  // Build the list and append the event.
  base::Value records_list{base::Value::Type::LIST};
  records_list.Append(std::move(event));

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
  base::Value event_list = ConvertRecordProtoToEventList(record);
  return policy::RealtimeReportingJobConfiguration::BuildReport(
      std::move(event_list), std::move(context));
}

}  // namespace reporting
