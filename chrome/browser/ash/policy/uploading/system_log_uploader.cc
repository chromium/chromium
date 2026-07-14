// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_policy_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_request_headers.h"

namespace policy {

namespace {

// The maximum number of successive retries.
const int kMaxNumRetries = 1;

// String constant defining the url tail we upload system logs to.
constexpr char kSystemLogUploadUrlTail[] = "/upload";

// Pseudo-location of policy dump file. Policy is uploaded from memory,
// there is no actual file on disk.
constexpr char kPolicyDumpFileLocation[] = "/var/log/policy_dump.json";

// Returns the system log upload frequency.
base::TimeDelta GetUploadFrequency() {
  base::TimeDelta upload_frequency(
      base::Milliseconds(SystemLogUploader::kDefaultUploadDelayMs));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kSystemLogUploadFrequency)) {
    std::string string_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            ash::switches::kSystemLogUploadFrequency);
    int frequency;
    if (base::StringToInt(string_value, &frequency)) {
      upload_frequency = base::Milliseconds(frequency);
    }
  }
  return upload_frequency;
}

std::string GetUploadUrl() {
  return g_browser_process->browser_policy_connector()
             ->GetDeviceManagementUrl() +
         kSystemLogUploadUrlTail;
}

}  // namespace

// Determines the time between log uploads.
const int64_t SystemLogUploader::kDefaultUploadDelayMs =
    12 * 60 * 60 * 1000;  // 12 hours

// Determines the time, measured from the time of last failed upload,
// after which the log upload is retried.
const int64_t SystemLogUploader::kErrorUploadDelayMs =
    120 * 1000;  // 120 seconds

// Determines max number of logs to be uploaded in kLogThrottleWindowDuration.
const int64_t SystemLogUploader::kLogThrottleCount = 100;

// Determines the time window for which the upload times should be stored.
const base::TimeDelta SystemLogUploader::kLogThrottleWindowDuration =
    base::Hours(24);

// The request header to attach the command ID to upload request. The command Id
// will be included in uploads that are triggered by
// `DeviceCommandFetchStatusJob`.
const char* const SystemLogUploader::kCommandIdHeaderName = "Command-ID";

// String constant identifying the header field which stores the file type.
const char* const SystemLogUploader::kFileTypeHeaderName = "File-Type";

// String constant signalling that the data segment contains zipped log files.
const char* const SystemLogUploader::kFileTypeZippedLogFile = "zipped_log_file";

// String constant for zipped logs name.
const char* const SystemLogUploader::kZippedLogsName = "logs";

// Name used for file containing zip archive of the logs.
const char* const SystemLogUploader::kZippedLogsFileName = "logs.zip";

// String constant signalling that the segment contains a binary file.
const char* const SystemLogUploader::kContentTypeOctetStream =
    "application/octet-stream";

SystemLogUploader::SystemLogUploader(
    PrefService* local_state,
    std::unique_ptr<Delegate> syslog_delegate,
    const scoped_refptr<base::SequencedTaskRunner>& task_runner)
    : local_state_(CHECK_DEREF(local_state)),
      retry_count_(0),
      upload_frequency_(GetUploadFrequency()),
      task_runner_(task_runner),
      syslog_delegate_(std::move(syslog_delegate)),
      upload_enabled_(false) {
  CHECK(syslog_delegate_);
  SYSLOG(INFO) << "Creating system log uploader.";

  // Watch for policy changes.
  upload_enabled_subscription_ = ash::CrosSettings::Get()->AddSettingsObserver(
      ash::kSystemLogUploadEnabled,
      base::BindRepeating(&SystemLogUploader::RefreshUploadSettings,
                          base::Unretained(this)));

  // Fetch the current value of the policy. This will also schedule a
  // system log upload if uploads become enabled.
  RefreshUploadSettings();
}

SystemLogUploader::~SystemLogUploader() = default;

void SystemLogUploader::OnSuccess() {
  SYSLOG(INFO) << "Upload successful.";
  upload_job_.reset();
  last_upload_attempt_ = base::Time::NowFromSystemTime();
  log_upload_in_progress_ = false;
  retry_count_ = 0;

  // On successful log upload schedule the next log upload after
  // upload_frequency_ time from now.
  ScheduleNextSystemLogUpload(upload_frequency_, std::nullopt);
}

void SystemLogUploader::OnFailure(UploadJob::ErrorCode error_code) {
  upload_job_.reset();
  last_upload_attempt_ = base::Time::NowFromSystemTime();
  log_upload_in_progress_ = false;

  //  If we have hit the maximum number of retries, terminate this upload
  //  attempt and schedule the next one using the normal delay. Otherwise, retry
  //  uploading after kErrorUploadDelayMs milliseconds.
  if (retry_count_++ < kMaxNumRetries) {
    SYSLOG(ERROR) << "Upload failed with error code " << error_code
                  << ", retrying later.";
    ScheduleNextSystemLogUpload(base::Milliseconds(kErrorUploadDelayMs),
                                std::nullopt);
  } else {
    // No more retries.
    SYSLOG(ERROR) << "Upload failed with error code " << error_code
                  << ", no more retries.";
    retry_count_ = 0;
    ScheduleNextSystemLogUpload(upload_frequency_, std::nullopt);
  }
}

// static
std::string SystemLogUploader::RemoveSensitiveData(
    redaction::RedactionTool* redactor,
    const std::string& data) {
  return redactor->Redact(data);
}

void SystemLogUploader::ScheduleNextSystemLogUploadImmediately(
    RemoteCommandJob::UniqueIDType command_id) {
  ScheduleNextSystemLogUpload(base::TimeDelta(), command_id);
}

void SystemLogUploader::RefreshUploadSettings() {
  // Attempt to fetch the current value of the reporting settings.
  // If trusted values are not available, register this function to be called
  // back when they are available.
  ash::CrosSettings* settings = ash::CrosSettings::Get();
  auto trust_status = settings->PrepareTrustedValues(base::BindOnce(
      &SystemLogUploader::RefreshUploadSettings, weak_factory_.GetWeakPtr()));
  if (trust_status != ash::CrosSettingsProvider::TRUSTED)
    return;

  // CrosSettings are trusted - we want to use the last trusted values, by
  // default do not upload system logs.
  // We also want to schedule a job if the settings switch to enabled, so
  // store the previous value.
  const bool previous_upload_enabled = upload_enabled_;
  if (!settings->GetBoolean(ash::kSystemLogUploadEnabled, &upload_enabled_)) {
    upload_enabled_ = false;
  }

  // Schedule a log upload job if uploads were previously disabled and
  // are now enabled. If no jobs have been attempted (ie. last_upload_attempt_
  // is the initial value) it will be scheduled immediately.
  if (!previous_upload_enabled && upload_enabled_){
    ScheduleNextSystemLogUpload(upload_frequency_, std::nullopt);
  }
}

void SystemLogUploader::UploadZippedSystemLogs(
    std::optional<RemoteCommandJob::UniqueIDType> command_id,
    std::string zipped_system_logs) {
  // Must be called on the main thread.
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!upload_job_);

  if (zipped_system_logs.empty()) {
    SYSLOG(ERROR) << "No zipped log to upload";
    return;
  }

  SYSLOG(INFO) << "Uploading zipped system logs.";

  GURL upload_url(GetUploadUrl());
  DCHECK(upload_url.is_valid());
  upload_job_ = syslog_delegate_->CreateUploadJob(upload_url, this);

  // Start a system log upload.
  std::map<std::string, std::string> header_fields;
  std::unique_ptr<std::string> data =
      std::make_unique<std::string>(zipped_system_logs);
  header_fields.insert(
      std::make_pair(kFileTypeHeaderName, kFileTypeZippedLogFile));
  header_fields.insert(std::make_pair(net::HttpRequestHeaders::kContentType,
                                      kContentTypeOctetStream));
  if (command_id) {
    header_fields.insert(std::make_pair(
        kCommandIdHeaderName, base::NumberToString(command_id.value())));
  }
  upload_job_->AddDataSegment(kZippedLogsName, kZippedLogsFileName,
                              header_fields, std::move(data));
  upload_job_->Start();
}

void SystemLogUploader::StartLogUpload(
    std::optional<RemoteCommandJob::UniqueIDType> command_id) {
  // Must be called on the main thread.
  DCHECK(thread_checker_.CalledOnValidThread());

  if (upload_enabled_) {
    SYSLOG(INFO) << "Reading system logs for upload.";
    log_upload_in_progress_ = true;
    syslog_delegate_->LoadSystemLogs(
        base::BindOnce(&SystemLogUploader::OnSystemLogsLoaded,
                       weak_factory_.GetWeakPtr(), std::move(command_id)));
  } else {
    // If upload is disabled, schedule the next attempt after 12h.
    SYSLOG(INFO) << "System log upload is disabled, rescheduling.";
    retry_count_ = 0;
    last_upload_attempt_ = base::Time::NowFromSystemTime();
    ScheduleNextSystemLogUpload(upload_frequency_, std::nullopt);
  }
}

void SystemLogUploader::OnSystemLogsLoaded(
    std::optional<RemoteCommandJob::UniqueIDType> command_id,
    std::unique_ptr<SystemLogs> system_logs) {
  // Must be called on the main thread.
  DCHECK(thread_checker_.CalledOnValidThread());
  system_logs->push_back(std::make_pair(kPolicyDumpFileLocation,
                                        syslog_delegate_->GetPolicyAsJSON()));

  SYSLOG(INFO) << "Starting zipped system log upload.";
  syslog_delegate_->ZipSystemLogs(
      std::move(system_logs),
      base::BindOnce(&SystemLogUploader::UploadZippedSystemLogs,
                     weak_factory_.GetWeakPtr(), std::move(command_id)));
}

// Update the list of logs within kLogThrottleWindowDuration window and add the
// latest log upload time if any.
base::Time SystemLogUploader::UpdateLocalStateForLogs() {
  const base::Time now = base::Time::NowFromSystemTime();

  const base::ListValue& prev_log_uploads =
      local_state_->GetList(ash::prefs::kStoreLogStatesAcrossReboots);

  std::vector<base::Time> updated_log_uploads;

  for (const base::Value& item : prev_log_uploads) {
    // ListValue stores Value type and Value does not support base::Time,
    // so we store double and convert to base::Time here.
    const base::Time current_item_time =
        base::Time::FromSecondsSinceUnixEpoch(item.GetDouble());

    // Logs are valid only if they occur in previous kLogThrottleWindowDuration
    // time window.
    if (now - current_item_time <= kLogThrottleWindowDuration)
      updated_log_uploads.push_back(current_item_time);
  }

  if (!last_upload_attempt_.is_null() &&
      (updated_log_uploads.empty() ||
       last_upload_attempt_ > updated_log_uploads.back())) {
    updated_log_uploads.push_back(last_upload_attempt_);
  }

  // This happens only in case of ScheduleNextSystemLogUploadImmediately. It is
  // sufficient to delete only one entry as at most 1 entry is appended on the
  // method call, hence the list size would exceed by at most 1.
  if (updated_log_uploads.size() > kLogThrottleCount)
    updated_log_uploads.erase(updated_log_uploads.begin());

  // Create a list to be updated for the pref.
  base::ListValue updated_prev_log_uploads;
  for (auto it : updated_log_uploads) {
    updated_prev_log_uploads.Append(it.InSecondsFSinceUnixEpoch());
  }
  local_state_->SetList(ash::prefs::kStoreLogStatesAcrossReboots,
                        std::move(updated_prev_log_uploads));

  // Write the changes to the disk to prevent loss of changes.
  local_state_->CommitPendingWrite();
  // If there are no log entries till now, return zero value.
  return updated_log_uploads.empty() ? base::Time() : updated_log_uploads[0];
}

void SystemLogUploader::ScheduleNextSystemLogUpload(
    base::TimeDelta frequency,
    std::optional<RemoteCommandJob::UniqueIDType> command_id) {
  // Don't schedule a new system log upload if there's a log upload in progress
  // (it will be scheduled once the current one completes).
  if (log_upload_in_progress_) {
    SYSLOG(INFO) << "In the middle of a system log upload, not scheduling the "
                 << "next one until this one finishes.";
    return;
  }
  base::Time last_valid_log_upload = UpdateLocalStateForLogs();
  // Calculate when to fire off the next update.
  base::TimeDelta delay = std::max(
      (last_upload_attempt_ + frequency) - base::Time::NowFromSystemTime(),
      base::TimeDelta());

  // To ensure at most kLogThrottleCount logs are uploaded in
  // kLogThrottleWindowDuration time.
  if (local_state_->GetList(ash::prefs::kStoreLogStatesAcrossReboots).size() >=
          kLogThrottleCount &&
      !frequency.is_zero()) {
    delay = std::max(delay, last_valid_log_upload + kLogThrottleWindowDuration -
                                base::Time::NowFromSystemTime());
  }

  SYSLOG(INFO) << "Scheduling next system log upload " << delay << " from now.";
  // Ensure that we never have more than one pending delayed task
  // (InvalidateWeakPtrs() will cancel any pending calls to log uploads).
  weak_factory_.InvalidateWeakPtrs();
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SystemLogUploader::StartLogUpload,
                     weak_factory_.GetWeakPtr(), command_id),
      delay);
}

}  // namespace policy
