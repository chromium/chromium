// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/system_log_uploader_delegate.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"
#include "chrome/browser/ash/policy/uploading/upload_job_impl.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/policy/chrome_policy_conversions_client.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/policy/core/browser/policy_conversions.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/core_account_id.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/zlib/google/zip.h"
#include "url/gurl.h"

namespace policy {

namespace {

// The cutoff point (in bytes) after which log contents are ignored.
constexpr size_t kLogCutoffSize = 50 * 1024 * 1024;  // 50 MiB.

// The file names of the system logs to upload.
// Note: do not add anything to this list without checking for PII in the file.
constexpr const char* kSystemLogFileNames[] = {
    "/var/log/bios_info.txt",
    "/var/log/chrome/chrome",
    "/var/log/chrome/chrome.PREVIOUS",
    "/var/log/eventlog.txt",
    "/var/log/extensions.log",
    "/var/log/extensions.1.log",
    "/var/log/messages",
    "/var/log/messages.1",
    "/var/log/net.log",
    "/var/log/net.1.log",
    "/var/log/ui/ui.LATEST",
    "/var/log/update_engine.log"};

std::string ZipFiles(
    std::unique_ptr<SystemLogUploader::SystemLogs> system_logs) {
  base::ScopedTempDir temp_dir;
  base::FilePath zip_file;
  std::string compressed_logs;
  auto zipped_logs = std::make_unique<SystemLogUploader::SystemLogs>();

  if (!temp_dir.CreateUniqueTempDir()) {
    return compressed_logs;
  }

  for (const auto& syslog_entry : *system_logs) {
    base::FilePath file_name = base::FilePath(syslog_entry.first).BaseName();
    base::FilePath file_path(temp_dir.GetPath().Append(file_name));
    if (!base::WriteFile(file_path, syslog_entry.second)) {
      PLOG(ERROR) << "Can't write log file: " << file_path.value();
      continue;
    }
  }
  system_logs.reset();

  if (!base::CreateTemporaryFile(&zip_file)) {
    PLOG(ERROR) << "Failed to create file to store zipped logs";
    return compressed_logs;
  }
  if (!zip::Zip(/*src_dir=*/temp_dir.GetPath(), /*dest_file=*/zip_file,
                /*include_hidden_files=*/false)) {
    SYSLOG(ERROR) << "Failed to zip system logs";
    base::DeleteFile(zip_file);
    return compressed_logs;
  }
  if (!base::ReadFileToString(zip_file, &compressed_logs)) {
    PLOG(ERROR) << "Failed to read zipped system logs";
    base::DeleteFile(zip_file);
    return compressed_logs;
  }
  base::DeleteFile(zip_file);
  return compressed_logs;
}

std::string ReadAndRedactLogFile(redaction::RedactionTool* redactor,
                                 const base::FilePath& file_path) {
  std::string data;
  if (!base::ReadFileToStringWithMaxSize(file_path, &data, kLogCutoffSize) &&
      data.empty()) {
    SYSLOG(ERROR) << "Failed to read the system log file from the disk "
                  << file_path.value();
  }
  // We want to remove the last line completely because PII data might be cut in
  // half (redactor might not recognize it).
  if (!data.empty() && data.back() != '\n') {
    size_t pos = data.find_last_of('\n');
    data.erase(pos != std::string::npos ? pos + 1 : 0);
    data += "... [truncated]\n";
  }
  return SystemLogUploader::RemoveSensitiveData(redactor, data);
}

// Reads the system log files as binary files, redacts data, stores the files
// as pairs (file name, data) and returns. Called on blocking thread.
std::unique_ptr<SystemLogUploader::SystemLogs> ReadFiles() {
  auto system_logs = std::make_unique<SystemLogUploader::SystemLogs>();
  redaction::RedactionTool redactor(
      extension_misc::kBuiltInFirstPartyExtensionIds);
  redactor.EnableCreditCardRedaction(true);
  for (const char* file_path : kSystemLogFileNames) {
    if (!base::PathExists(base::FilePath(file_path))) {
      continue;
    }
    system_logs->push_back(std::make_pair(
        file_path, ReadAndRedactLogFile(&redactor, base::FilePath(file_path))));
  }
  return system_logs;
}

}  // namespace

SystemLogUploaderDelegate::SystemLogUploaderDelegate(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : url_loader_factory_(std::move(url_loader_factory)),
      task_runner_(std::move(task_runner)) {
  CHECK(url_loader_factory_);
}

SystemLogUploaderDelegate::~SystemLogUploaderDelegate() = default;

std::string SystemLogUploaderDelegate::GetPolicyAsJSON() {
  bool include_user_policies = false;
  if (user_manager::UserManager::IsInitialized()) {
    if (user_manager::UserManager::Get()->GetPrimaryUser()) {
      include_user_policies =
          user_manager::UserManager::Get()->GetPrimaryUser()->IsAffiliated();
    }
  }

  return PolicyConversions(std::make_unique<ChromePolicyConversionsClient>(
                               ProfileManager::GetActiveUserProfile()))
      .EnableUserPolicies(include_user_policies)
      .EnableDeviceLocalAccountPolicies(true)
      .EnableDeviceInfo(true)
      .ToJSON();
}

void SystemLogUploaderDelegate::LoadSystemLogs(
    LogUploadCallback upload_callback) {
  // Run ReadFiles() in the thread that interacts with the file system and
  // return system logs to |upload_callback| on the current thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ReadFiles), std::move(upload_callback));
}

std::unique_ptr<UploadJob> SystemLogUploaderDelegate::CreateUploadJob(
    const GURL& upload_url,
    UploadJob::Delegate* delegate) {
  DeviceOAuth2TokenService* device_oauth2_token_service =
      DeviceOAuth2TokenServiceFactory::Get();

  CoreAccountId robot_account_id =
      device_oauth2_token_service->GetRobotAccountId();

  SYSLOG(INFO) << "Creating upload job for system log";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("policy_system_logs", R"(
        semantics {
          sender: "Chrome OS system log uploader"
          description:
              "Admins can ask that their devices regularly upload their system "
              "logs."
          trigger: "After reboot and every 12 hours."
          data: "Non-user specific, redacted system logs from /var/log/."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          chrome_policy {
            LogUploadEnabled {
                LogUploadEnabled: false
            }
          }
        }
      )");
  return std::make_unique<UploadJobImpl>(
      upload_url, robot_account_id,
      device_oauth2_token_service->GetAccessTokenManager(), url_loader_factory_,
      delegate, std::make_unique<UploadJobImpl::RandomMimeBoundaryGenerator>(),
      traffic_annotation, task_runner_);
}

void SystemLogUploaderDelegate::ZipSystemLogs(
    std::unique_ptr<SystemLogUploader::SystemLogs> system_logs,
    ZippedLogUploadCallback upload_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ZipFiles, std::move(system_logs)),
      std::move(upload_callback));
}

}  // namespace policy
