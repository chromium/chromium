// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_

#include <set>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {
class ContextualTasksService;
enum class ContextualTaskContextSource;

}  // namespace contextual_tasks

class AimEligibilityService;

namespace contextual_tasks {

class ContextualTasksContextControllerImpl
    : public ContextualTasksContextController {
 public:
  ContextualTasksContextControllerImpl(
      ContextualTasksService* service,
      AimEligibilityService* aim_eligibility_service);
  ~ContextualTasksContextControllerImpl() override;

  // ContextualTasksContextController implementation.
  ContextualTask CreateTask() override;
  void AssignThreadToTask(const base::Uuid& task_id,
                          ThreadType thread_type,
                          const std::string& server_id,
                          const std::string& conversation_turn_id,
                          std::optional<std::string> title) override;
  void UpdateThreadTurnId(const base::Uuid& task_id,
                          ThreadType thread_type,
                          const std::string& server_id,
                          const std::string& conversation_turn_id) override;
  void GetTasks(
      base::OnceCallback<void(std::vector<ContextualTask>)> callback) override;
  void GetTask(const base::Uuid& task_id,
               base::OnceCallback<void(std::optional<ContextualTask>)> callback)
      override;
  void AssociateTabWithTask(SessionID tab_session_id,
                            const base::Uuid& task_id) override;
  void GetSelectedTaskForTab(
      SessionID tab_session_id,
      base::OnceCallback<void(std::optional<ContextualTask>)>
          selected_task_callback) override;
  void AttachUrlToTask(const base::Uuid& task_id, const GURL& url) override;
  void DetachUrlFromTask(const base::Uuid& task_id, const GURL& url) override;
  void GetContextForTask(
      const base::Uuid& task_id,
      const std::set<ContextualTaskContextSource>& sources,
      base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
          context_callback) override;
  FeatureEligibility GetFeatureEligibility() override;

 private:
  raw_ptr<ContextualTasksService> service_;
  raw_ptr<AimEligibilityService> aim_eligibility_service_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_IMPL_H_
