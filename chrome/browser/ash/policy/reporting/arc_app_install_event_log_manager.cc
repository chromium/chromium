// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_manager.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/app_list/arc/arc_app_utils.h"
#include "chrome/browser/profiles/profile.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The log file, stored in the user's profile directory.
constexpr base::FilePath::CharType kLogFileName[] =
    FILE_PATH_LITERAL("app_push_install_log");

// Returns the path to the app push-install event log file for |profile|.
base::FilePath GetLogFilePath(const Profile& profile) {
  return profile.GetPath().Append(kLogFileName);
}

}  // namespace

ArcAppInstallEventLogManager::ArcAppInstallEventLogManager(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    ArcAppInstallEventLogUploader* uploader,
    Profile* profile)
    : InstallEventLogManagerBase(log_task_runner_wrapper, profile),
      uploader_(uploader) {
  uploader_->SetDelegate(this);
  log_ = std::make_unique<ArcLog>();
  app_log_upload_ = std::make_unique<AppLogUpload>(this);
  log_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcLog::Init, base::Unretained(log_.get()),
                     GetLogFilePath(*profile)),
      base::BindOnce(&ArcAppInstallEventLogManager::AppLogUpload::OnLogInit,
                     app_log_upload_->log_weak_factory_.GetWeakPtr()));
  logger_ = std::make_unique<ArcAppInstallEventLogger>(this, profile);
}

ArcAppInstallEventLogManager::~ArcAppInstallEventLogManager() {
  logger_.reset();
  // Destroy |app_log_upload_| after |logger_| otherwise it is possible
  // that when we add new logs created through |logger_| cause the crash as
  // |app_log_upload_| will be destroyed already.
  app_log_upload_.reset();
  uploader_->SetDelegate(nullptr);
  log_task_runner_->DeleteSoon(FROM_HERE, std::move(log_));
}

// static
void ArcAppInstallEventLogManager::Clear(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    Profile* profile) {
  ArcAppInstallEventLogger::Clear(profile);
  log_task_runner_wrapper->GetTaskRunner()->PostTask(
      FROM_HERE, base::GetDeleteFileCallback(GetLogFilePath(*profile)));
}

void ArcAppInstallEventLogManager::Add(
    const std::set<std::string>& packages,
    const em::AppInstallReportLogEvent& event) {
  if (packages.empty()) {
    return;
  }
  // If --arc-install-event-chrome-log-for-tests is present, write event logs to
  // Chrome log (/var/log/chrome). LOG(ERROR) ensures that logs are written.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kArcInstallEventChromeLogForTests)) {
    for (std::string package : packages)
      LOG(ERROR) << "Add ARC install event: " << package << ", "
                 << event.event_type();
  }

  log_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcLog::Add, base::Unretained(log_.get()), packages,
                     event),
      base::BindOnce(&ArcAppInstallEventLogManager::AppLogUpload::OnLogChange,
                     app_log_upload_->log_weak_factory_.GetWeakPtr()));
}

void ArcAppInstallEventLogManager::GetAndroidId(
    ArcAppInstallEventLogger::Delegate::AndroidIdCallback callback) const {
  arc::GetAndroidId(std::move(callback));
}

void ArcAppInstallEventLogManager::SerializeForUpload(
    ArcAppInstallEventLogUploader::Delegate::SerializationCallback callback) {
  log_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ArcAppInstallEventLogManager::ArcLog::Serialize,
                     base::Unretained(log_.get())),
      base::BindOnce(
          &ArcAppInstallEventLogManager::LogUpload::OnSerializeLogDone<
              ArcAppInstallEventLogUploader::Delegate::SerializationCallback,
              em::AppInstallReportRequest>,
          app_log_upload_->log_weak_factory_.GetWeakPtr(),
          std::move(callback)));
}

void ArcAppInstallEventLogManager::OnUploadSuccess() {
  if (app_log_upload_->store_scheduled_) {
    app_log_upload_->store_scheduled_ = false;
    app_log_upload_->store_weak_factory_.InvalidateWeakPtrs();
  }
  app_log_upload_->upload_requested_ = false;

  log_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ArcAppInstallEventLogManager::ArcLog::ClearSerializedAndStore,
          base::Unretained(log_.get())),
      base::BindOnce(&ArcAppInstallEventLogManager::AppLogUpload::OnLogChange,
                     app_log_upload_->log_weak_factory_.GetWeakPtr()));
}

ArcAppInstallEventLogManager::ArcLog::ArcLog() : InstallLog() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ArcAppInstallEventLogManager::ArcLog::~ArcLog() {}

std::unique_ptr<em::AppInstallReportRequest>
ArcAppInstallEventLogManager::ArcLog::Serialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  auto serialized_log = std::make_unique<em::AppInstallReportRequest>();
  log_->Serialize(serialized_log.get());
  return serialized_log;
}

ArcAppInstallEventLogManager::AppLogUpload::AppLogUpload(
    ArcAppInstallEventLogManager* owner)
    : owner_(owner) {}
ArcAppInstallEventLogManager::AppLogUpload::~AppLogUpload() = default;

void ArcAppInstallEventLogManager::AppLogUpload::StoreLog() {
  CHECK(owner_->log_);
  store_scheduled_ = false;
  owner_->log_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ArcLog::Store, base::Unretained(owner_->log_.get())));
}

void ArcAppInstallEventLogManager::AppLogUpload::RequestUploadForUploader() {
  owner_->uploader_->RequestUpload();
}

}  // namespace policy
