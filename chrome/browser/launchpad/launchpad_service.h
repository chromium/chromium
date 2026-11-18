// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
#define CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_

#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/launchpad/launchpad_task.h"
#include "chrome/browser/launchpad/tasks_store.h"
#include "components/keyed_service/core/keyed_service.h"

namespace personal_context {
struct FetchContextResult;
class PersonalContextService;
}  // namespace personal_context

namespace launchpad {

class LaunchpadService : public KeyedService {
 public:
  explicit LaunchpadService(
      personal_context::PersonalContextService* personal_context_service);

  LaunchpadService(const LaunchpadService&) = delete;
  LaunchpadService& operator=(const LaunchpadService&) = delete;
  ~LaunchpadService() override;

  using GenerateTasksCallback = base::OnceCallback<void(
      const std::optional<std::vector<LaunchpadTask>>&)>;

  // Generates tasks, saves them in `tasks_store_`, and invokes `callback` on
  // completion (whether successful or not), unless `LaunchpadService` is
  // destroyed first.
  void GenerateTasks(GenerateTasksCallback callback);

  // Returns true if a task generation request is currently in flight.
  // TODO(b/586300169): Have the WebUI check `IsGeneratingTasks()` on load and
  // observe generation state changes so controls are disabled while
  // generating/writing.
  bool IsGeneratingTasks() const;

  // Retrieves all tasks currently stored in `tasks_store_`.
  // TODO(b/586300169): Read from server cache when implemented.
  void GetAllTasks(TasksStore::GetAllTasksCallback callback) const;

 private:
  // Handles the async response from the AutoTodos fetch.
  void OnTasksFetched(GenerateTasksCallback callback,
                      personal_context::FetchContextResult result);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;

  TasksStore tasks_store_;

  // Indicates if a `GenerateTasks()` request is currently in flight, enforcing
  // that only a single generation flow is active at a time.
  bool is_generating_tasks_ = false;

  base::WeakPtrFactory<LaunchpadService> weak_factory_{this};
};

}  // namespace launchpad

#endif  // CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
