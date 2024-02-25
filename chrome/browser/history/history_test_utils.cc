// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/history/history_service_factory.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "content/public/test/test_utils.h"

namespace {

// Note: WaitableEvent is not used for synchronization between the main thread
// and history backend thread because the history subsystem posts tasks back
// to the main thread. Had we tried to Signal an event in such a task
// and Wait for it on the main thread, the task would not run at all because
// the main thread would be blocked on the Wait call, resulting in a deadlock.

// A task to be scheduled on the history backend thread.
// Notifies the main thread after all history backend thread tasks have run.
class WaitForHistoryTask : public history::HistoryDBTask {
 public:
  explicit WaitForHistoryTask(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  WaitForHistoryTask(const WaitForHistoryTask&) = delete;
  WaitForHistoryTask& operator=(const WaitForHistoryTask&) = delete;

  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    return true;
  }
  void DoneRunOnMainThread() override { std::move(quit_closure_).Run(); }

 private:
  base::OnceClosure quit_closure_;
  ~WaitForHistoryTask() override {}
};

}  // namespace

void WaitForHistoryBackendToRun(Profile* profile) {
  base::RunLoop loop;
  base::CancelableTaskTracker task_tracker;
  std::unique_ptr<history::HistoryDBTask> task(
      new WaitForHistoryTask(loop.QuitWhenIdleClosure()));
  history::HistoryService* history = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  history->ScheduleDBTask(FROM_HERE, std::move(task), &task_tracker);
  loop.Run();
}
