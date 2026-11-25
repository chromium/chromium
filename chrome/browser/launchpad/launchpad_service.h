// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
#define CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chrome/browser/launchpad/launchpad_task.h"
#include "chrome/browser/launchpad/tasks_store.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserWindowInterface;
class Profile;

namespace personal_context {
struct FetchContextResult;
class PersonalContextService;
}  // namespace personal_context

namespace launchpad {

class LaunchpadService : public KeyedService {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Currently invoked when a generation completes and stores updated tasks.
    // TODO(b/586299803): Also notify on user task status mutations.
    virtual void OnTasksChanged(base::span<const LaunchpadTask> tasks) = 0;
    virtual void OnTasksGenerationStateChanged(
        const TasksGenerationMetadata& metadata) = 0;
  };

  LaunchpadService(
      Profile* profile,
      personal_context::PersonalContextService* personal_context_service);

  LaunchpadService(const LaunchpadService&) = delete;
  LaunchpadService& operator=(const LaunchpadService&) = delete;
  ~LaunchpadService() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  using GenerateTasksCallback = base::OnceCallback<void(
      const std::optional<std::vector<LaunchpadTask>>&)>;

  // Generates tasks, saves them in `tasks_store_`, and invokes `callback` on
  // completion (whether successful or not), unless `LaunchpadService` is
  // destroyed first.
  // While the UI normally disables generation buttons during an active request,
  // concurrent calls coalesce onto the in-flight request and receive its result
  // to handle edge cases (e.g. multiple open surfaces, IPC state races, or
  // non-UI callers) without issuing duplicate fetches.
  void GenerateTasks(GenerateTasksCallback callback);

  // Returns true if a task generation request is currently in flight.
  bool IsGeneratingTasks() const;

  // Returns the current generation state, last success timestamp, and error
  // status for Launchpad tasks.
  const TasksGenerationMetadata& GetGenerationMetadata() const;

  // Retrieves all tasks currently stored in `tasks_store_`.
  // TODO(b/586300169): Read from server cache when implemented.
  void GetAllTasks(TasksStore::GetAllTasksCallback callback) const;

  // Invokes Glic with auto-submit in actuation (AutoBrowse) mode to execute
  // `prompt` in `browser_window`. Returns true if the invocation was
  // dispatched, or false if `prompt` is empty or Glic is unavailable.
  bool RunTask(const std::string& prompt,
               BrowserWindowInterface* browser_window);

  // Temporary helper for legacy `kAutoTodos` callers when `kLaunchpad` (and
  // thus `LaunchpadService`) is disabled.
  // TODO(b/587591147): Remove once `kAutoTodos` callers are cleaned up.
  static bool RunTaskForProfile(Profile* profile,
                                const std::string& prompt,
                                BrowserWindowInterface* browser_window);

 private:
  // Handles the async response from the AutoTodos fetch.
  void OnTasksFetched(personal_context::FetchContextResult result);

  // Completes `GenerateTasks()` after newly generated tasks are stored.
  void OnGeneratedTasksStored(std::vector<LaunchpadTask> stored_tasks);

  // Clears generation state, notifies observers, and resolves pending
  // `GenerateTasks()` callbacks with `result`.
  void CompleteTaskGeneration(std::optional<std::vector<LaunchpadTask>> result);

  const raw_ref<Profile> profile_;
  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;

  TasksStore tasks_store_;

  // Tracks whether a generation request is in flight, the last completion
  // timestamp, and whether the last attempt failed.
  TasksGenerationMetadata generation_metadata_;

  // Callbacks waiting on the currently in-flight `GenerateTasks()` request.
  // Coalesces edge-case concurrent calls (e.g. across multiple open UI
  // surfaces or IPC races before the UI disables the button) onto one fetch.
  std::vector<GenerateTasksCallback> pending_generate_callbacks_;

  base::ObserverList<Observer, /*check_empty=*/true> observers_;

  base::WeakPtrFactory<LaunchpadService> weak_factory_{this};
};

}  // namespace launchpad

#endif  // CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
