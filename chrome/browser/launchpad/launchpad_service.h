// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
#define CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/launchpad/tasks_store.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/personal_context/proto/features/auto_todos.pb.h"

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
      std::optional<personal_context::proto::AutoTodosResponse>)>;

  // Generates tasks and invokes `callback` on completion (whether successful
  // or not), unless `LaunchpadService` is destroyed first.
  void GenerateTasks(GenerateTasksCallback callback);

 private:
  // Handles the async response from the AutoTodos fetch.
  void OnTasksFetched(GenerateTasksCallback callback,
                      personal_context::FetchContextResult result);

  const raw_ref<personal_context::PersonalContextService>
      personal_context_service_;

  TasksStore tasks_store_;

  base::WeakPtrFactory<LaunchpadService> weak_factory_{this};
};

}  // namespace launchpad

#endif  // CHROME_BROWSER_LAUNCHPAD_LAUNCHPAD_SERVICE_H_
