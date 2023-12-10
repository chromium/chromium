// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_screenshot_job.h"

#include <fstream>
#include <optional>
#include <utility>

#include "ash/shell.h"
#include "base/barrier_callback.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_request_headers.h"

namespace policy {

namespace {

using ResultCode = DeviceCommandScreenshotJob::ResultCode;

// String constant identifying the result field in the result payload.
const char* const kResultFieldName = "result";

// Template string constant for populating the name field.
const char* const kNameFieldTemplate = "Screen %zu";

// Template string constant for populating the name field.
const char* const kFilenameFieldTemplate = "screen%zu.png";

// String constant identifying the header field which stores the command id.
const char* const kCommandIdHeaderName = "Command-ID";

// String constant signalling that the segment contains a png image.
const char* const kContentTypeImagePng = "image/png";

// String constant identifying the header field which stores the file type.
const char* const kFileTypeHeaderName = "File-Type";

// String constant signalling that the data segment contains screenshots.
const char* const kFileTypeScreenshotFile = "screenshot_file";

// Helper method to hide the |screen_index| and `std::make_pair` from the
// |DeviceCommandScreenshotJob::Delegate|.
void CallCollectAndUpload(
    base::OnceCallback<void(ScreenshotData)> collect_and_upload,
    size_t screen_index,
    scoped_refptr<base::RefCountedMemory> png_data) {
  std::move(collect_and_upload).Run(std::make_pair(screen_index, png_data));
}

std::string CreatePayload(ResultCode result_code) {
  base::Value::Dict root_dict;
  if (result_code != ResultCode::SUCCESS) {
    root_dict.Set(kResultFieldName, result_code);
  }

  return base::WriteJson(root_dict).value();
}

ResultCode ToResultCode(UploadJob::ErrorCode error_code) {
  switch (error_code) {
    case UploadJob::AUTHENTICATION_ERROR:
      return ResultCode::FAILURE_AUTHENTICATION;
    case UploadJob::NETWORK_ERROR:
    case UploadJob::SERVER_ERROR:
      return ResultCode::FAILURE_SERVER;
  }
}

}  // namespace

// String constant identifying the upload url field in the command payload.
constexpr char DeviceCommandScreenshotJob::kUploadUrlFieldName[] =
    "fileUploadUrl";

DeviceCommandScreenshotJob::DeviceCommandScreenshotJob(
    std::unique_ptr<Delegate> screenshot_delegate)
    : screenshot_delegate_(std::move(screenshot_delegate)) {
  DCHECK(screenshot_delegate_);
}

DeviceCommandScreenshotJob::~DeviceCommandScreenshotJob() = default;

enterprise_management::RemoteCommand_Type DeviceCommandScreenshotJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_SCREENSHOT;
}

void DeviceCommandScreenshotJob::OnSuccess() {
  SYSLOG(INFO) << "Upload successful.";
  ReportResult(ResultType::kSuccess, ResultCode::SUCCESS);
}

void DeviceCommandScreenshotJob::OnFailure(UploadJob::ErrorCode error_code) {
  SYSLOG(ERROR) << "Upload failure: " << error_code;
  ReportResult(ResultType::kFailure, ToResultCode(error_code));
}

bool DeviceCommandScreenshotJob::ParseCommandPayload(
    const std::string& command_payload) {
  std::optional<base::Value> root(base::JSONReader::Read(command_payload));
  if (!root || !root->is_dict()) {
    return false;
  }
  const std::string* upload_url =
      root->GetDict().FindString(kUploadUrlFieldName);
  if (!upload_url) {
    return false;
  }
  upload_url_ = GURL(*upload_url);
  return true;
}

void DeviceCommandScreenshotJob::OnScreenshotsReady(
    scoped_refptr<base::TaskRunner> task_runner,
    std::vector<ScreenshotData> upload_data) {
  // TODO(https://crbug.com/1297571) Do we really need to re-post here?
  // Can we add guarantees to
  // `DeviceCommandScreenshotJob::Delegate::TakeSnapshot`?
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&DeviceCommandScreenshotJob::StartScreenshotUpload,
                     weak_ptr_factory_.GetWeakPtr(), std::move(upload_data)));
}

void DeviceCommandScreenshotJob::StartScreenshotUpload(
    std::vector<ScreenshotData> upload_data) {
  std::sort(begin(upload_data), end(upload_data),
            [](const auto& l, const auto& r) { return l.first < r.first; });

  for (const auto& screenshot_entry : upload_data) {
    if (!screenshot_entry.second) {
      LOG(WARNING) << "not uploading empty screenshot at index "
                   << screenshot_entry.first;
      continue;
    }
    std::map<std::string, std::string> header_fields;
    header_fields.insert(
        std::make_pair(kFileTypeHeaderName, kFileTypeScreenshotFile));
    header_fields.insert(std::make_pair(net::HttpRequestHeaders::kContentType,
                                        kContentTypeImagePng));
    header_fields.insert(std::make_pair(kCommandIdHeaderName,
                                        base::NumberToString(unique_id())));
    std::unique_ptr<std::string> data = std::make_unique<std::string>(
        (const char*)screenshot_entry.second->front(),
        screenshot_entry.second->size());
    upload_job_->AddDataSegment(
        base::StringPrintf(kNameFieldTemplate, screenshot_entry.first),
        base::StringPrintf(kFilenameFieldTemplate, screenshot_entry.first),
        header_fields, std::move(data));
  }
  upload_job_->Start();
}

void DeviceCommandScreenshotJob::ReportResult(ResultType result_type,
                                              ResultCode result_code) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_callback_), result_type,
                                CreatePayload(result_code)));
}

void DeviceCommandScreenshotJob::RunImpl(CallbackWithResult result_callback) {
  result_callback_ = std::move(result_callback);

  SYSLOG(INFO) << "Executing screenshot command.";

  // Fail if the delegate says screenshots are not allowed in this session.
  if (!screenshot_delegate_->IsScreenshotAllowed()) {
    SYSLOG(ERROR) << "Screenshots are not allowed.";
    ReportResult(ResultType::kFailure, ResultCode::FAILURE_USER_INPUT);
    return;
  }

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  // Immediately fail if the upload url is invalid.
  if (!upload_url_.is_valid()) {
    SYSLOG(ERROR) << upload_url_ << " is not a valid URL.";
    ReportResult(ResultType::kFailure, ResultCode::FAILURE_INVALID_URL);
    return;
  }

  // Immediately fail if there are no attached screens.
  if (root_windows.size() == 0) {
    SYSLOG(ERROR) << "No attached screens.";
    ReportResult(ResultType::kFailure,
                 ResultCode::FAILURE_SCREENSHOT_ACQUISITION);
    return;
  }

  upload_job_ = screenshot_delegate_->CreateUploadJob(upload_url_, this);
  DCHECK(upload_job_);

  // Post tasks to the sequenced worker pool for taking screenshots on each
  // attached screen.
  auto collect_and_upload = base::BarrierCallback<ScreenshotData>(
      root_windows.size(),
      base::BindOnce(&DeviceCommandScreenshotJob::OnScreenshotsReady,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::SingleThreadTaskRunner::GetCurrentDefault()));
  for (size_t screen_index = 0; screen_index < root_windows.size();
       ++screen_index) {
    aura::Window* root_window = root_windows[screen_index];
    gfx::Rect rect = root_window->bounds();
    screenshot_delegate_->TakeSnapshot(
        root_window, rect,
        base::BindOnce(CallCollectAndUpload, collect_and_upload, screen_index));
  }
}

void DeviceCommandScreenshotJob::TerminateImpl() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace policy
