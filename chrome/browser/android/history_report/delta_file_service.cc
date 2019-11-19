// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/history_report/delta_file_service.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/trace_event/memory_dump_manager.h"
#include "chrome/browser/android/history_report/delta_file_backend_leveldb.h"
#include "chrome/browser/android/history_report/delta_file_commons.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

void DeltaFileServiceDoAddPage(history_report::DeltaFileBackend* backend,
                               const GURL& url) {
  backend->PageAdded(url);
}

void DeltaFileServiceDoDeletePage(history_report::DeltaFileBackend* backend,
                                  const GURL& url) {
  backend->PageDeleted(url);
}

void DeltaFileServiceDoTrim(history_report::DeltaFileBackend* backend,
                            int64_t lower_bound,
                            base::WaitableEvent* finished,
                            int64_t* result) {
  *result = backend->Trim(lower_bound);
  finished->Signal();
}

void DeltaFileServiceDoQuery(
    history_report::DeltaFileBackend* backend,
    int64_t last_seq_no,
    int32_t limit,
    base::WaitableEvent* finished,
    std::unique_ptr<std::vector<history_report::DeltaFileEntryWithData>>*
        result) {
  *result = backend->Query(last_seq_no, limit);
  finished->Signal();
}

void DeltaFileServiceDoRecreate(history_report::DeltaFileBackend* backend,
                                const std::vector<std::string>& urls,
                                base::WaitableEvent* finished,
                                bool* result) {
  *result = backend->Recreate(urls);
  finished->Signal();
}

void DeltaFileServiceDoClear(history_report::DeltaFileBackend* backend) {
  backend->Clear();
}

void DeltaFileServiceDoDump(history_report::DeltaFileBackend* backend,
                            base::WaitableEvent* finished,
                            std::string* result) {
  result->append(backend->Dump());
  finished->Signal();
}

void DeltaFileServiceDoUnregisterMDP(
    std::unique_ptr<history_report::DeltaFileBackend> backend) {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      backend.get());
}

}  // namespace

namespace history_report {

using content::BrowserThread;

DeltaFileService::DeltaFileService(const base::FilePath& dir)
    : task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      delta_file_backend_(new DeltaFileBackend(dir)) {
  base::trace_event::MemoryDumpManager::GetInstance()
      ->RegisterDumpProviderWithSequencedTaskRunner(
          delta_file_backend_.get(), "HistoryReport", task_runner_,
          base::trace_event::MemoryDumpProvider::Options());
}

DeltaFileService::~DeltaFileService() {
  // Unregister should happen on task runner.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&DeltaFileServiceDoUnregisterMDP,
                                        std::move(delta_file_backend_)));
}

void DeltaFileService::PageAdded(const GURL& url) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeltaFileServiceDoAddPage,
                     base::Unretained(delta_file_backend_.get()), url));
}

void DeltaFileService::PageDeleted(const GURL& url) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeltaFileServiceDoDeletePage,
                     base::Unretained(delta_file_backend_.get()), url));
}

int64_t DeltaFileService::Trim(int64_t lower_bound) {
  int64_t result;
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeltaFileServiceDoTrim,
                     base::Unretained(delta_file_backend_.get()), lower_bound,
                     base::Unretained(&finished), base::Unretained(&result)));
  finished.Wait();
  return result;
}

std::unique_ptr<std::vector<DeltaFileEntryWithData>> DeltaFileService::Query(
    int64_t last_seq_no,
    int32_t limit) {
  std::unique_ptr<std::vector<DeltaFileEntryWithData>> result;
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeltaFileServiceDoQuery,
                                base::Unretained(delta_file_backend_.get()),
                                last_seq_no, limit, base::Unretained(&finished),
                                base::Unretained(&result)));
  finished.Wait();
  return result;
}

bool DeltaFileService::Recreate(const std::vector<std::string>& urls) {
  bool result = false;
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeltaFileServiceDoRecreate,
                     base::Unretained(delta_file_backend_.get()), urls,
                     base::Unretained(&finished), base::Unretained(&result)));
  finished.Wait();
  return result;
}

void DeltaFileService::Clear() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DeltaFileServiceDoClear,
                                base::Unretained(delta_file_backend_.get())));
}

std::string DeltaFileService::Dump() {
  std::string dump;
  base::WaitableEvent finished(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DeltaFileServiceDoDump,
                     base::Unretained(delta_file_backend_.get()),
                     base::Unretained(&finished), base::Unretained(&dump)));
  finished.Wait();
  return dump;
}

}  // namespace history_report
