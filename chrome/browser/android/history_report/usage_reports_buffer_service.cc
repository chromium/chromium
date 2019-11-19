// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/usage_reports_buffer_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/trace_event/memory_dump_manager.h"
#include "chrome/browser/android/history_report/usage_reports_buffer_backend.h"
#include "chrome/browser/android/proto/delta_file.pb.h"


namespace {

void UsageReportsBufferServiceDoInit(
    history_report::UsageReportsBufferBackend* backend) {
  backend->Init();
}

void UsageReportsBufferServiceDoAddVisit(
    history_report::UsageReportsBufferBackend* backend,
    const std::string id,
    int64_t timestamp_ms,
    bool typed_visit) {
  backend->AddVisit(id, timestamp_ms, typed_visit);
}

void UsageReportsBufferServiceDoRemove(
    history_report::UsageReportsBufferBackend* backend,
    const std::vector<std::string>* reports,
    base::WaitableEvent* finished) {
  backend->Remove(*reports);
  finished->Signal();
}

void UsageReportsBufferServiceDoGetUsageReportsBatch(
    history_report::UsageReportsBufferBackend* backend,
    int32_t batch_size,
    base::WaitableEvent* finished,
    std::unique_ptr<std::vector<history_report::UsageReport>>* result) {
  *result = backend->GetUsageReportsBatch(batch_size);
  finished->Signal();
}

void UsageReportsBufferServiceDoClear(
    history_report::UsageReportsBufferBackend* backend,
    base::WaitableEvent* finished) {
  backend->Clear();
  finished->Signal();
}

void UsageReportsBufferServiceDoDump(
    history_report::UsageReportsBufferBackend* backend,
    base::WaitableEvent* finished,
    std::string* result) {
  result->append(backend->Dump());
  finished->Signal();
}

void UsageReportsBufferServiceDoUnregisterMDP(
    std::unique_ptr<history_report::UsageReportsBufferBackend> backend) {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      backend.get());
}

}  // namespace

namespace history_report {

UsageReportsBufferService::UsageReportsBufferService(const base::FilePath& dir)
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      backend_(new UsageReportsBufferBackend(dir)) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          backend_.get(), "HistoryReport", task_runner_,
          base::trace_event::MemoryDumpProvider::Options());
}

UsageReportsBufferService::~UsageReportsBufferService() {
  // Unregister should happen on task runner.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&UsageReportsBufferServiceDoUnregisterMDP,
                                std::move(backend_)));
}

void UsageReportsBufferService::Init() {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&UsageReportsBufferServiceDoInit,
                                        base::Unretained(backend_.get())));
}

void UsageReportsBufferService::AddVisit(const std::string& id,
                                         int64_t timestamp_ms,
                                         bool typed_visit) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&UsageReportsBufferServiceDoAddVisit,
                                        base::Unretained(backend_.get()), id,
                                        timestamp_ms, typed_visit));
}

std::unique_ptr<std::vector<UsageReport>>
UsageReportsBufferService::GetUsageReportsBatch(int32_t batch_size) {
  std::unique_ptr<std::vector<UsageReport>> result;
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  // It's ok to pass unretained pointers here because this is a synchronous
  // call.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsageReportsBufferServiceDoGetUsageReportsBatch,
                     base::Unretained(backend_.get()), batch_size,
                     base::Unretained(&finished), base::Unretained(&result)));
  finished.Wait();
  return result;
}

void UsageReportsBufferService::Remove(
    const std::vector<std::string>& report_ids) {
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  // It's ok to pass unretained pointers here because this is a synchronous
  // call.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&UsageReportsBufferServiceDoRemove,
                                        base::Unretained(backend_.get()),
                                        base::Unretained(&report_ids),
                                        base::Unretained(&finished)));
  finished.Wait();
}

void UsageReportsBufferService::Clear() {
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  // It's ok to pass unretained pointers here because this is a synchronous
  // call.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&UsageReportsBufferServiceDoClear,
                                        base::Unretained(backend_.get()),
                                        base::Unretained(&finished)));
  finished.Wait();
}

std::string UsageReportsBufferService::Dump() {
  std::string dump;
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  // It's ok to pass unretained pointers here because this is a synchronous
  // call.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&UsageReportsBufferServiceDoDump,
                     base::Unretained(backend_.get()),
                     base::Unretained(&finished), base::Unretained(&dump)));
  finished.Wait();
  return dump;
}

}  // namespace history_report
