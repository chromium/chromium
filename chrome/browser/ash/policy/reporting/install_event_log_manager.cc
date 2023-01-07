// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/install_event_log_manager.h"

#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"

namespace policy {

namespace {

// Delay after which a change to the log contents is stored to disk. Further
// changes during this time window are picked up by the same store operation.
constexpr base::TimeDelta kStoreDelay = base::Seconds(5);
// Reduce store delay for integration tests.
constexpr base::TimeDelta kFastStoreDelay = base::Seconds(1);

// Interval between subsequent uploads to the server, if the log is not empty.
constexpr base::TimeDelta kUploadInterval = base::Hours(3);

// Delay of an expedited upload to the server, used for the first upload after
// the |InstallEventLogManagerBase| is constructed and whenever the log is
// getting full.
constexpr base::TimeDelta kExpeditedUploadDelay = base::Minutes(15);
// Reduce upload delay for integration tests.
constexpr base::TimeDelta kFastUploadDelay = base::Seconds(30);

// An expedited upload is scheduled whenever the total number of log entries
// exceeds |kTotalSizeExpeditedUploadThreshold| or the number of log entries for
// any single app exceeds |kMaxSizeExpeditedUploadThreshold|.
constexpr int kTotalSizeExpeditedUploadThreshold = 2048;
constexpr int kMaxSizeExpeditedUploadThreshold = 512;

// If install-log-fast-upload-for-tests flag is enabled, upload logs with
// reduced delay.
bool FastUploadForTestsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      ash::switches::kInstallLogFastUploadForTests);
}

}  // namespace

InstallEventLogManagerBase::LogTaskRunnerWrapper::LogTaskRunnerWrapper() =
    default;

InstallEventLogManagerBase::LogTaskRunnerWrapper::~LogTaskRunnerWrapper() =
    default;

scoped_refptr<base::SequencedTaskRunner>
InstallEventLogManagerBase::LogTaskRunnerWrapper::GetTaskRunner() {
  if (!task_runner_) {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  }

  return task_runner_;
}

InstallEventLogManagerBase::InstallEventLogManagerBase(
    LogTaskRunnerWrapper* log_task_runner_wrapper,
    Profile* profile)
    : log_task_runner_(log_task_runner_wrapper->GetTaskRunner()) {}

InstallEventLogManagerBase::~InstallEventLogManagerBase() {}

InstallEventLogManagerBase::LogUpload::LogUpload() = default;
InstallEventLogManagerBase::LogUpload::~LogUpload() = default;

void InstallEventLogManagerBase::LogUpload::OnLogInit(const LogSize& log_size) {
  OnLogChange(log_size);
  EnsureUpload(true /* expedited */);
}

void InstallEventLogManagerBase::LogUpload::OnLogChange(
    const LogSize& log_size) {
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
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&InstallEventLogManagerBase::LogUpload::StoreLog,
                       store_weak_factory_.GetWeakPtr()),
        FastUploadForTestsEnabled() ? kFastStoreDelay : kStoreDelay);
  }

  EnsureUpload(log_size.total_size > kTotalSizeExpeditedUploadThreshold ||
               log_size.max_size >
                   kMaxSizeExpeditedUploadThreshold /* expedited*/);
}

void InstallEventLogManagerBase::LogUpload::EnsureUpload(bool expedited) {
  if (upload_requested_ ||
      (upload_scheduled_ && (expedited_upload_scheduled_ || !expedited))) {
    return;
  }

  if (upload_scheduled_)
    upload_weak_factory_.InvalidateWeakPtrs();
  upload_scheduled_ = true;
  expedited_upload_scheduled_ = expedited;

  base::TimeDelta upload_delay =
      expedited ? kExpeditedUploadDelay : kUploadInterval;
  if (FastUploadForTestsEnabled())
    upload_delay = kFastUploadDelay;

  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&InstallEventLogManagerBase::LogUpload::RequestUpload,
                     upload_weak_factory_.GetWeakPtr()),
      upload_delay);
}

void InstallEventLogManagerBase::LogUpload::RequestUpload() {
  upload_scheduled_ = false;
  expedited_upload_scheduled_ = false;
  if (!log_size_.total_size)
    return;
  upload_requested_ = true;
  RequestUploadForUploader();
}

}  // namespace policy
