// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/remote_commands/device_command_screenshot_job.h"

#include <fstream>
#include <utility>

#include "ash/shell.h"
#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/upload_job_impl.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"

namespace policy {

namespace {

// String constant identifying the result field in the result payload.
const char* const kResultFieldName = "result";

// Template string constant for populating the name field.
const char* const kNameFieldTemplate = "Screen %d";

// Template string constant for populating the name field.
const char* const kFilenameFieldTemplate = "screen%d.png";

// String constant identifying the header field which stores the command id.
const char* const kCommandIdHeaderName = "Command-ID";

// String constant signalling that the segment contains a png image.
const char* const kContentTypeImagePng = "image/png";

// String constant identifying the header field which stores the file type.
const char* const kFileTypeHeaderName = "File-Type";

// String constant signalling that the data segment contains screenshots.
const char* const kFileTypeScreenshotFile = "screenshot_file";

// String constant identifying the upload url field in the command payload.
const char* const kUploadUrlFieldName = "fileUploadUrl";

// A helper function which invokes |store_screenshot_callback| on |task_runner|.
void RunStoreScreenshotOnTaskRunner(
    ui::GrabWindowSnapshotAsyncPNGCallback store_screenshot_callback,
    scoped_refptr<base::TaskRunner> task_runner,
    scoped_refptr<base::RefCountedMemory> png_data) {
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(store_screenshot_callback), png_data));
}

}  // namespace

class DeviceCommandScreenshotJob::Payload
    : public RemoteCommandJob::ResultPayload {
 public:
  explicit Payload(ResultCode result_code);
  ~Payload() override {}

  // RemoteCommandJob::ResultPayload:
  std::unique_ptr<std::string> Serialize() override;

 private:
  std::string payload_;

  DISALLOW_COPY_AND_ASSIGN(Payload);
};

DeviceCommandScreenshotJob::Payload::Payload(ResultCode result_code) {
  base::DictionaryValue root_dict;
  if (result_code != SUCCESS)
    root_dict.SetInteger(kResultFieldName, result_code);
  base::JSONWriter::Write(root_dict, &payload_);
}

std::unique_ptr<std::string> DeviceCommandScreenshotJob::Payload::Serialize() {
  return std::make_unique<std::string>(payload_);
}

DeviceCommandScreenshotJob::DeviceCommandScreenshotJob(
    std::unique_ptr<Delegate> screenshot_delegate)
    : num_pending_screenshots_(0),
      screenshot_delegate_(std::move(screenshot_delegate)) {
  DCHECK(screenshot_delegate_);
}

DeviceCommandScreenshotJob::~DeviceCommandScreenshotJob() {
}

enterprise_management::RemoteCommand_Type DeviceCommandScreenshotJob::GetType()
    const {
  return enterprise_management::RemoteCommand_Type_DEVICE_SCREENSHOT;
}

void DeviceCommandScreenshotJob::OnSuccess() {
  SYSLOG(INFO) << "Upload successful.";
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(succeeded_callback_),
                                std::make_unique<Payload>(SUCCESS)));
}

void DeviceCommandScreenshotJob::OnFailure(UploadJob::ErrorCode error_code) {
  SYSLOG(ERROR) << "Upload failure: " << error_code;
  ResultCode result_code = FAILURE_CLIENT;
  switch (error_code) {
    case UploadJob::AUTHENTICATION_ERROR:
      result_code = FAILURE_AUTHENTICATION;
      break;
    case UploadJob::NETWORK_ERROR:
    case UploadJob::SERVER_ERROR:
      result_code = FAILURE_SERVER;
      break;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(failed_callback_),
                                std::make_unique<Payload>(result_code)));
}

bool DeviceCommandScreenshotJob::ParseCommandPayload(
    const std::string& command_payload) {
  std::unique_ptr<base::Value> root(
      base::JSONReader().ReadToValueDeprecated(command_payload));
  if (!root.get())
    return false;
  base::DictionaryValue* payload = nullptr;
  if (!root->GetAsDictionary(&payload))
    return false;
  std::string upload_url;
  if (!payload->GetString(kUploadUrlFieldName, &upload_url))
    return false;
  upload_url_ = GURL(upload_url);
  return true;
}

void DeviceCommandScreenshotJob::StoreScreenshot(
    size_t screen,
    scoped_refptr<base::RefCountedMemory> png_data) {
  screenshots_.insert(std::make_pair(screen, png_data));
  DCHECK_LT(0, num_pending_screenshots_);
  --num_pending_screenshots_;

  if (num_pending_screenshots_ == 0)
    StartScreenshotUpload();
}

void DeviceCommandScreenshotJob::StartScreenshotUpload() {
  for (const auto& screenshot_entry : screenshots_) {
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

void DeviceCommandScreenshotJob::RunImpl(CallbackWithResult succeeded_callback,
                                         CallbackWithResult failed_callback) {
  succeeded_callback_ = std::move(succeeded_callback);
  failed_callback_ = std::move(failed_callback);

  SYSLOG(INFO) << "Executing screenshot command.";

  // Fail if the delegate says screenshots are not allowed in this session.
  if (!screenshot_delegate_->IsScreenshotAllowed()) {
    SYSLOG(ERROR) << "Screenshots are not allowed.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failed_callback_),
                       std::make_unique<Payload>(FAILURE_USER_INPUT)));
  }

  aura::Window::Windows root_windows = ash::Shell::GetAllRootWindows();

  // Immediately fail if the upload url is invalid.
  if (!upload_url_.is_valid()) {
    SYSLOG(ERROR) << upload_url_ << " is not a valid URL.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(failed_callback_),
                       std::make_unique<Payload>(FAILURE_INVALID_URL)));
    return;
  }

  // Immediately fail if there are no attached screens.
  if (root_windows.size() == 0) {
    SYSLOG(ERROR) << "No attached screens.";
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(failed_callback_),
                                  std::make_unique<Payload>(
                                      FAILURE_SCREENSHOT_ACQUISITION)));
    return;
  }

  upload_job_ = screenshot_delegate_->CreateUploadJob(upload_url_, this);
  DCHECK(upload_job_);

  // Post tasks to the sequenced worker pool for taking screenshots on each
  // attached screen.
  num_pending_screenshots_ = root_windows.size();
  for (size_t screen = 0; screen < root_windows.size(); ++screen) {
    aura::Window* root_window = root_windows[screen];
    gfx::Rect rect = root_window->bounds();
    screenshot_delegate_->TakeSnapshot(
        root_window, rect,
        base::BindOnce(
            &RunStoreScreenshotOnTaskRunner,
            base::BindOnce(&DeviceCommandScreenshotJob::StoreScreenshot,
                           weak_ptr_factory_.GetWeakPtr(), screen),
            base::ThreadTaskRunnerHandle::Get()));
  }
}

void DeviceCommandScreenshotJob::TerminateImpl() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace policy
