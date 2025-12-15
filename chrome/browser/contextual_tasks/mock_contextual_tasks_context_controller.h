// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_tasks {

class MockContextualTasksContextController
    : public ContextualTasksContextController {
 public:
  MockContextualTasksContextController();
  MockContextualTasksContextController(
      const MockContextualTasksContextController&) = delete;
  MockContextualTasksContextController& operator=(
      const MockContextualTasksContextController&) = delete;
  ~MockContextualTasksContextController() override;

  MOCK_METHOD(ContextualTask, CreateTask, (), (override));
  MOCK_METHOD(ContextualTask, CreateTaskFromUrl, (const GURL& url), (override));
  MOCK_METHOD(
      void,
      GetTaskById,
      (const base::Uuid& task_id,
       base::OnceCallback<void(std::optional<ContextualTask>)> callback),
      (const, override));
  MOCK_METHOD(void,
              GetTasks,
              (base::OnceCallback<void(std::vector<ContextualTask>)> callback),
              (const, override));
  MOCK_METHOD(void, DeleteTask, (const base::Uuid& task_id), (override));
  MOCK_METHOD(void,
              UpdateThreadForTask,
              (const base::Uuid& task_id,
               ThreadType thread_type,
               const std::string& server_id,
               std::optional<std::string> conversation_turn_id,
               std::optional<std::string> title),
              (override));
  MOCK_METHOD(std::optional<ContextualTask>,
              GetTaskFromServerId,
              (ThreadType thread_type, const std::string& server_id),
              (override));
  MOCK_METHOD(void,
              RemoveThreadFromTask,
              (const base::Uuid& task_id,
               ThreadType type,
               const std::string& server_id),
              (override));
  MOCK_METHOD(void,
              AttachUrlToTask,
              (const base::Uuid& task_id, const GURL& url),
              (override));
  MOCK_METHOD(void,
              DetachUrlFromTask,
              (const base::Uuid& task_id, const GURL& url),
              (override));
  MOCK_METHOD(void,
              SetUrlResourcesFromServer,
              (const base::Uuid& task_id,
               std::vector<UrlResource> url_resources),
              (override));
  MOCK_METHOD(void,
              GetContextForTask,
              (const base::Uuid& task_id,
               const std::set<ContextualTaskContextSource>& sources,
               std::unique_ptr<ContextDecorationParams> params,
               base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                   context_callback),
              (override));
  MOCK_METHOD(void,
              AssociateTabWithTask,
              (const base::Uuid& task_id, SessionID tab_id),
              (override));
  MOCK_METHOD(void,
              DisassociateTabFromTask,
              (const base::Uuid& task_id, SessionID tab_id),
              (override));
  MOCK_METHOD(void,
              DisassociateAllTabsFromTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(std::vector<SessionID>,
              GetTabsAssociatedWithTask,
              (const base::Uuid& task_id),
              (const, override));
  MOCK_METHOD(std::optional<ContextualTask>,
              GetContextualTaskForTab,
              (SessionID tab_id),
              (const, override));
  MOCK_METHOD(void,
              ClearAllTabAssociationsForTask,
              (const base::Uuid& task_id),
              (override));

  MOCK_METHOD(void, AddObserver, (Observer * observer), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer * observer), (override));
  MOCK_METHOD(base::WeakPtr<syncer::DataTypeControllerDelegate>,
              GetAiThreadControllerDelegate,
              (),
              (override));
  MOCK_METHOD(FeatureEligibility, GetFeatureEligibility, (), (override));
  MOCK_METHOD(bool, IsInitialized, (), (override));
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_MOCK_CONTEXTUAL_TASKS_CONTEXT_CONTROLLER_H_
