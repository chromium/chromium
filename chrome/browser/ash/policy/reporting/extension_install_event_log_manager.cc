// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/extension_install_event_log_manager.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/task_runner_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_registry.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The log file, stored in the user's profile directory.
constexpr base::FilePath::CharType kLogFileName[] =
    FILE_PATH_LITERAL("extension_install_log");

// Returns the path to the extension install event log file for |profile|.
base::FilePath GetLogFilePath(const Profile& profile) {
  return profile.GetPath().Append(kLogFileName);
}

}  // namespace

ExtensionInstallEventLogManager::ExtensionInstallEventLogManager(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    ExtensionInstallEventLogUploader* uploader,
    Profile* profile)
    : InstallEventLogManagerBase(log_task_runner_wrapper, profile),
      uploader_(uploader) {
  uploader_->SetDelegate(this);
  log_ = std::make_unique<ExtensionLog>();
  extension_log_upload_ = std::make_unique<ExtensionLogUpload>(this);
  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ExtensionLog::Init, base::Unretained(log_.get()),
                     GetLogFilePath(*profile)),
      base::BindOnce(
          &ExtensionInstallEventLogManager::ExtensionLogUpload::OnLogInit,
          extension_log_upload_->log_weak_factory_.GetWeakPtr()));
  logger_ = std::make_unique<ExtensionInstallEventLogger>(
      this, profile, extensions::ExtensionRegistry::Get(profile));
}

ExtensionInstallEventLogManager::~ExtensionInstallEventLogManager() {
  logger_.reset();
  // Destroy |extension_log_upload_| after |logger_| otherwise it is possible
  // that when we add new logs created through |logger_| cause the crash as
  // |extension_log_upload_| will be destroyed already.
  extension_log_upload_.reset();
  uploader_->SetDelegate(nullptr);
  log_task_runner_->DeleteSoon(FROM_HERE, std::move(log_));
}

// static
void ExtensionInstallEventLogManager::Clear(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    Profile* profile) {
  log_task_runner_wrapper->GetTaskRunner()->PostTask(
      FROM_HERE, base::GetDeleteFileCallback(GetLogFilePath(*profile)));
}

void ExtensionInstallEventLogManager::Add(
    std::set<extensions::ExtensionId> extensions,
    const em::ExtensionInstallReportLogEvent& event) {
  if (extensions.empty())
    return;
  // After |logger_| is destroyed, we do not add any new events. And |log_| is
  // destroyed by creating a non nestable task at the end of
  // ExtensionInstallEventLogManager destructor, it is safe to use
  // base::Unretained(log_.get()) here.
  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ExtensionLog::Add, base::Unretained(log_.get()),
                     std::move(extensions), event),
      base::BindOnce(
          &ExtensionInstallEventLogManager::ExtensionLogUpload::OnLogChange,
          extension_log_upload_->log_weak_factory_.GetWeakPtr()));
}

void ExtensionInstallEventLogManager::SerializeExtensionLogForUpload(
    ExtensionInstallEventLogUploader::Delegate::
        ExtensionLogSerializationCallback callback) {
  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ExtensionInstallEventLogManager::ExtensionLog::Serialize,
                     base::Unretained(log_.get())),
      base::BindOnce(
          &ExtensionInstallEventLogManager::LogUpload::OnSerializeLogDone<
              ExtensionInstallEventLogUploader::Delegate::
                  ExtensionLogSerializationCallback,
              em::ExtensionInstallReportRequest>,
          extension_log_upload_->log_weak_factory_.GetWeakPtr(),
          std::move(callback)));
}

void ExtensionInstallEventLogManager::OnExtensionLogUploadSuccess() {
  if (extension_log_upload_->store_scheduled_) {
    extension_log_upload_->store_scheduled_ = false;
    extension_log_upload_->store_weak_factory_.InvalidateWeakPtrs();
  }
  extension_log_upload_->upload_requested_ = false;

  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ExtensionInstallEventLogManager::ExtensionLog::
                         ClearSerializedAndStore,
                     base::Unretained(log_.get())),
      base::BindOnce(
          &ExtensionInstallEventLogManager::ExtensionLogUpload::OnLogChange,
          extension_log_upload_->log_weak_factory_.GetWeakPtr()));
}

ExtensionInstallEventLogManager::ExtensionLog::ExtensionLog() : InstallLog() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ExtensionInstallEventLogManager::ExtensionLog::~ExtensionLog() {}

std::unique_ptr<em::ExtensionInstallReportRequest>
ExtensionInstallEventLogManager::ExtensionLog::Serialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  auto serialized_log = std::make_unique<em::ExtensionInstallReportRequest>();
  log_->Serialize(serialized_log.get());
  return serialized_log;
}

ExtensionInstallEventLogManager::ExtensionLogUpload::ExtensionLogUpload(
    ExtensionInstallEventLogManager* owner)
    : owner_(owner) {}
ExtensionInstallEventLogManager::ExtensionLogUpload::~ExtensionLogUpload() =
    default;

void ExtensionInstallEventLogManager::ExtensionLogUpload::StoreLog() {
  CHECK(owner_->log_);
  store_scheduled_ = false;
  owner_->log_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&ExtensionLog::Store,
                                base::Unretained(owner_->log_.get())));
}

void ExtensionInstallEventLogManager::ExtensionLogUpload::
    RequestUploadForUploader() {
  owner_->uploader_->RequestUpload();
}

}  // namespace policy
