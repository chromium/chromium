// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job_test_util.h"

#include <algorithm>
#include <cinttypes>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/reporting/storage/test_storage_module.h"

namespace policy::test {

base::Value::Dict GetFetchSupportPacketCommandPayloadDict(
    const std::vector<support_tool::DataCollectorType>& data_collectors,
    const std::vector<support_tool::PiiType>& pii_types) {
  base::Value::Dict support_packet_details;
  support_packet_details.Set("issueCaseId", "issue_case_id");
  support_packet_details.Set("issueDescription", "issue description");
  base::Value::List data_collectors_list;
  for (const auto& data_collector : data_collectors) {
    data_collectors_list.Append(data_collector);
  }
  support_packet_details.Set("requestedDataCollectors",
                             std::move(data_collectors_list));
  base::Value::List pii_types_list;
  for (const auto& pii_type : pii_types) {
    pii_types_list.Append(pii_type);
  }
  support_packet_details.Set("requestedPiiTypes", std::move(pii_types_list));
  return base::Value::Dict().Set("supportPacketDetails",
                                 std::move(support_packet_details));
}

void CaptureUpcomingLogUploadEventOnReportingStorage(
    scoped_refptr<reporting::test::TestStorageModule> reporting_storage,
    base::RepeatingCallback<void(ash::reporting::LogUploadEvent)>
        event_callback) {
  // We bind `event_callback` to current task because `AddRecord()` is posted on
  // ThreadPool and `event_callback` will be triggered from there.
  event_callback = base::BindPostTaskToCurrentDefault(event_callback);
  EXPECT_CALL(*reporting_storage,
              AddRecord(testing::Eq(reporting::Priority::SLOW_BATCH),
                        testing::ResultOf(
                            [](const reporting::Record& record) {
                              return record.destination();
                            },
                            testing::Eq(reporting::Destination::LOG_UPLOAD)),
                        testing::_))
      .WillOnce(
          [event_callback](
              reporting::Priority priority, reporting::Record record,
              reporting::StorageModuleInterface::EnqueueCallback callback) {
            EXPECT_TRUE(record.needs_local_unencrypted_copy());
            ash::reporting::LogUploadEvent upcoming_event;
            EXPECT_TRUE(upcoming_event.ParseFromArray(record.data().data(),
                                                      record.data().size()));
            std::move(event_callback).Run(std::move(upcoming_event));
            std::move(callback).Run(reporting::Status::StatusOK());
          });
}

std::string GetExpectedUploadParameters(int64_t command_id,
                                        const std::string& exported_filepath) {
  constexpr char kExpectedUploadParametersFormatter[] =
      R"({"Command-ID":"%)" PRId64
      R"(","File-Type":"support_file","Filename":"%s"}
application/json)";
  return base::StringPrintf(kExpectedUploadParametersFormatter, command_id,
                            exported_filepath.c_str());
}

}  // namespace policy::test
