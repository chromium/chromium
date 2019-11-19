// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/policy/app_install_event_log.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_uploader.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

namespace {

// The log file, stored in the user's profile directory.
constexpr base::FilePath::CharType kLogFileName[] =
    FILE_PATH_LITERAL("app_push_install_log");

// Delay after which a change to the log contents is stored to disk. Further
// changes during this time window are picked up by the same store operation.
constexpr base::TimeDelta kStoreDelay = base::TimeDelta::FromSeconds(5);

// Interval between subsequent uploads to the server, if the log is not empty.
constexpr base::TimeDelta kUploadInterval = base::TimeDelta::FromHours(3);

// Delay of an expedited upload to the server, used for the first upload after
// the |AppInstallEventLogManager| is constructed and whenever the log is
// getting full.
constexpr base::TimeDelta kExpeditedUploadDelay =
    base::TimeDelta::FromMinutes(15);

// An expedited upload is scheduled whenever the total number of log entries
// exceeds |kTotalSizeExpeditedUploadThreshold| or the number of log entries for
// any single app exceeds |kMaxSizeExpeditedUploadThreshold|.
constexpr int kTotalSizeExpeditedUploadThreshold = 2048;
constexpr int kMaxSizeExpeditedUploadThreshold = 512;

// Returns the path to the app push-install event log file for |profile|.
base::FilePath GetLogFilePath(const Profile& profile) {
  return profile.GetPath().Append(kLogFileName);
}

}  // namespace

AppInstallEventLogManager::LogTaskRunnerWrapper::LogTaskRunnerWrapper() =
    default;

AppInstallEventLogManager::LogTaskRunnerWrapper::~LogTaskRunnerWrapper() =
    default;

scoped_refptr<base::SequencedTaskRunner>
AppInstallEventLogManager::LogTaskRunnerWrapper::GetTaskRunner() {
  if (!task_runner_) {
    task_runner_ = base::CreateSequencedTaskRunner(
        {base::ThreadPool(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
         base::MayBlock()});
  }

  return task_runner_;
}

AppInstallEventLogManager::AppInstallEventLogManager(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    AppInstallEventLogUploader* uploader,
    Profile* profile)
    : log_task_runner_(log_task_runner_wrapper->GetTaskRunner()),
      uploader_(uploader) {
  uploader_->SetDelegate(this);
  log_ = std::make_unique<Log>();
  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&Log::Init, base::Unretained(log_.get()),
                     GetLogFilePath(*profile)),
      base::BindOnce(&AppInstallEventLogManager::OnLogInit,
                     log_weak_factory_.GetWeakPtr()));
  logger_ = std::make_unique<AppInstallEventLogger>(this, profile);
}

AppInstallEventLogManager::~AppInstallEventLogManager() {
  logger_.reset();
  uploader_->SetDelegate(nullptr);
  log_task_runner_->DeleteSoon(FROM_HERE, std::move(log_));
}

// static
void AppInstallEventLogManager::Clear(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    Profile* profile) {
  AppInstallEventLogger::Clear(profile);
  log_task_runner_wrapper->GetTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](const base::FilePath& log_file_path) {
                       base::DeleteFile(log_file_path, false /* recursive */);
                     },
                     GetLogFilePath(*profile)));
}

void AppInstallEventLogManager::Add(const std::set<std::string>& packages,
                                    const em::AppInstallReportLogEvent& event) {
  if (packages.empty()) {
    return;
  }
  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&Log::Add, base::Unretained(log_.get()), packages, event),
      base::BindOnce(&AppInstallEventLogManager::OnLogChange,
                     log_weak_factory_.GetWeakPtr()));
}

void AppInstallEventLogManager::SerializeForUpload(
    AppInstallEventLogUploader::Delegate::SerializationCallback callback) {
  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&AppInstallEventLogManager::Log::Serialize,
                     base::Unretained(log_.get())),
      base::BindOnce(&AppInstallEventLogManager::OnSerializeLogDone,
                     log_weak_factory_.GetWeakPtr(), std::move(callback)));
}

void AppInstallEventLogManager::OnUploadSuccess() {
  if (store_scheduled_) {
    store_scheduled_ = false;
    store_weak_factory_.InvalidateWeakPtrs();
  }
  upload_requested_ = false;

  base::PostTaskAndReplyWithResult(
      log_task_runner_.get(), FROM_HERE,
      base::BindOnce(&AppInstallEventLogManager::Log::ClearSerializedAndStore,
                     base::Unretained(log_.get())),
      base::BindOnce(&AppInstallEventLogManager::OnLogChange,
                     log_weak_factory_.GetWeakPtr()));
}

AppInstallEventLogManager::Log::Log() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AppInstallEventLogManager::Log::~Log() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  log_->Store();
}

AppInstallEventLogManager::LogSize AppInstallEventLogManager::Log::Init(
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!log_);
  log_ = std::make_unique<AppInstallEventLog>(file_path);
  return GetSize();
}

AppInstallEventLogManager::LogSize AppInstallEventLogManager::Log::Add(
    const std::set<std::string>& packages,
    const em::AppInstallReportLogEvent& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  for (const auto& package : packages) {
    log_->Add(package, event);
  }
  return GetSize();
}

void AppInstallEventLogManager::Log::Store() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  log_->Store();
}

std::unique_ptr<em::AppInstallReportRequest>
AppInstallEventLogManager::Log::Serialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  auto serialized_log = std::make_unique<em::AppInstallReportRequest>();
  log_->Serialize(serialized_log.get());
  return serialized_log;
}

AppInstallEventLogManager::LogSize
AppInstallEventLogManager::Log::ClearSerializedAndStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  log_->ClearSerialized();
  log_->Store();
  return GetSize();
}

AppInstallEventLogManager::LogSize AppInstallEventLogManager::Log::GetSize()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogSize size;
  size.total_size = log_->total_size();
  size.max_size = log_->max_size();
  return size;
}

void AppInstallEventLogManager::OnLogInit(const LogSize& log_size) {
  OnLogChange(log_size);
  EnsureUpload(true /* expedited */);
}

void AppInstallEventLogManager::OnLogChange(const LogSize& log_size) {
  log_size_ = log_size;

  if (!log_size.total_size) {
    if (upload_scheduled_) {
      upload_scheduled_ = false;
      expedited_upload_scheduled_ = false;
      upload_weak_factory_.InvalidateWeakPtrs();
    }
    return;
  }

  if (!store_scheduled_) {
    store_scheduled_ = true;
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&AppInstallEventLogManager::StoreLog,
                       store_weak_factory_.GetWeakPtr()),
        kStoreDelay);
  }

  EnsureUpload(log_size.total_size > kTotalSizeExpeditedUploadThreshold ||
               log_size.max_size >
                   kMaxSizeExpeditedUploadThreshold /* expedited*/);
}

void AppInstallEventLogManager::OnSerializeLogDone(
    AppInstallEventLogUploader::Delegate::SerializationCallback callback,
    std::unique_ptr<em::AppInstallReportRequest> log) {
  std::move(callback).Run(log.get());
}

void AppInstallEventLogManager::StoreLog() {
  CHECK(log_);
  store_scheduled_ = false;
  log_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Log::Store, base::Unretained(log_.get())));
}

void AppInstallEventLogManager::EnsureUpload(bool expedited) {
  if (upload_requested_ ||
      (upload_scheduled_ && (expedited_upload_scheduled_ || !expedited))) {
    return;
  }

  if (upload_scheduled_) {
    upload_weak_factory_.InvalidateWeakPtrs();
  }
  upload_scheduled_ = true;
  expedited_upload_scheduled_ = expedited;

  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&AppInstallEventLogManager::RequestUpload,
                     upload_weak_factory_.GetWeakPtr()),
      expedited ? kExpeditedUploadDelay : kUploadInterval);
}

void AppInstallEventLogManager::RequestUpload() {
  upload_scheduled_ = false;
  expedited_upload_scheduled_ = false;
  if (!log_size_.total_size) {
    return;
  }
  upload_requested_ = true;
  uploader_->RequestUpload();
}

}  // namespace policy
