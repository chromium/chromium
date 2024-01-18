// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_TEST_UTIL_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "components/reporting/storage/test_storage_module.h"

namespace policy::test {

// Return a valid command payload with given data collectors requested.
// The returned payload doesn't contain any PII request. The returned payload
// will be as following.
// {"supportPacketDetails":{
//     "issueCaseId": "issue_case_id",
//     "issueDescription": "issue description",
//     "requestedDataCollectors": [<requested data collectors>],
//     "requestedPiiTypes": [],
//     "requesterMetadata": "obfuscated123"
//   }
// }
base::Value::Dict GetFetchSupportPacketCommandPayloadDict(
    const std::vector<support_tool::DataCollectorType>& data_collectors);

// Captures the next uploaded LogUploadEvent on the `reporting_storage` and
// returns it to `event_callback`. Sets an expectancy that a LogUploadEvent will
// be enqueued to `reporting_storage` and fails the test if it does now.
void CaptureUpcomingLogUploadEventOnReportingStorage(
    scoped_refptr<reporting::test::TestStorageModule> reporting_storage,
    base::RepeatingCallback<void(ash::reporting::LogUploadEvent)>
        event_callback);

// Returns the expected upload_parameters field of a LogUploadEvent for the
// given values. The expected upload_paramaters will be in following format:
//  {
//     "Command-ID":"<command ID>",
//     "File-Type":"support_file",
//     "Filename":"<the filepath for the generated log file>"}
// application/json
std::string GetExpectedUploadParameters(int64_t command_id,
                                        const std::string& exported_filepath);

}  // namespace policy::test

#endif  // CHROME_BROWSER_ASH_POLICY_REMOTE_COMMANDS_DEVICE_COMMAND_FETCH_SUPPORT_PACKET_JOB_TEST_UTIL_H_
