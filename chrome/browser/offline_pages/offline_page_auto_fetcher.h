// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_AUTO_FETCHER_H_
#define CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_AUTO_FETCHER_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/offline_pages/core/background/request_queue_results.h"
#include "components/offline_pages/core/background/save_page_request.h"

namespace content {
class BrowserContext;
}

namespace offline_pages {
class RequestCoordinator;

// Provides control of fetching pages for later in an automatic / quiet way.
// TODO(crbug.com/883486): This class is not yet used.
class OfflinePageAutoFetcher : public chrome::mojom::OfflinePageAutoFetcher {
 public:
  explicit OfflinePageAutoFetcher(content::BrowserContext* browser_context);
  ~OfflinePageAutoFetcher() override;
  void TrySchedule(bool user_requested,
                   const GURL& url,
                   TryScheduleCallback callback) override;
  void CancelSchedule(const GURL& url) override;

  bool IsTaskQueueEmptyForTesting();

 private:
  class TaskToken;
  using TaskCallback = base::OnceCallback<void(TaskToken)>;
  using OfflinePageAutoFetcherScheduleResult =
      chrome::mojom::OfflinePageAutoFetcherScheduleResult;
  // Task management methods. Each request made to this class is serialized by
  // appending the tasks as callbacks to task_queue_.

  // Starts or enqueues a new task.
  void StartOrEnqueue(TaskCallback task);
  // Reports a task complete. Must be called when the task is completely
  // finished.
  void TaskComplete(TaskToken token);

  base::WeakPtr<OfflinePageAutoFetcher> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Implementation details for public methods.

  void TryScheduleStep1(bool user_requested,
                        const GURL& url,
                        TryScheduleCallback callback,
                        TaskToken token);
  void TryScheduleStep2(
      TaskToken token,
      bool user_requested,
      const GURL& url,
      TryScheduleCallback callback,
      RequestCoordinator* coordinator,
      std::vector<std::unique_ptr<SavePageRequest>> all_requests);
  void TryScheduleStep3(TaskToken token,
                        TryScheduleCallback callback,
                        AddRequestResult result);

  void CancelScheduleStep1(const GURL& url, TaskToken token);
  void CancelScheduleStep2(
      TaskToken token,
      const GURL& url,
      RequestCoordinator* coordinator,
      std::vector<std::unique_ptr<SavePageRequest>> requests);
  void CancelScheduleStep3(TaskToken token, const MultipleItemStatuses&);

  content::BrowserContext* browser_context_;
  // TODO(harringtond): Pull out task management into another class, or use
  // offline_pages::TaskQueue.
  std::queue<TaskCallback> task_queue_;
  base::WeakPtrFactory<OfflinePageAutoFetcher> weak_ptr_factory_{this};
};

}  // namespace offline_pages

#endif  // CHROME_BROWSER_OFFLINE_PAGES_OFFLINE_PAGE_AUTO_FETCHER_H_
