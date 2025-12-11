// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace base {
class Uuid;
}

class ContextualTasksUI;

namespace contextual_tasks {
class ContextualTasksContextController;
class ContextualTasksUiService;
}  // namespace contextual_tasks

class ContextualTasksPageHandler
    : public contextual_tasks::mojom::PageHandler,
      public contextual_tasks::ContextualTasksService::Observer {
 public:
  ContextualTasksPageHandler(
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> receiver,
      ContextualTasksUI* web_ui_controller,
      contextual_tasks::ContextualTasksUiService* ui_service,
      contextual_tasks::ContextualTasksContextController* context_controller);
  ~ContextualTasksPageHandler() override;

  // contextual_tasks::mojom::PageHandler:
  void GetThreadUrl(GetThreadUrlCallback callback) override;
  void GetUrlForTask(const base::Uuid& uuid,
                     GetUrlForTaskCallback callback) override;
  void SetTaskId(const base::Uuid& uuid) override;
  void SetThreadTitle(const std::string& title) override;

  void CloseSidePanel() override;
  void ShowThreadHistory() override;
  void IsShownInTab(IsShownInTabCallback callback) override;
  void OpenMyActivityUi() override;
  void OpenHelpUi() override;
  void MoveTaskUiToNewTab() override;
  void OnTabClickedFromSourcesMenu(int32_t tab_id, const GURL& url) override;
  void OnWebviewMessage(const std::vector<uint8_t>& message) override;
  void GetCommonSearchParams(bool is_dark_mode,
                             bool is_side_panel,
                             GetCommonSearchParamsCallback callback) override;
  void PostMessageToWebview(const lens::ClientToAimMessage& message);

  // contextual_tasks::ContextualTasksService::Observer:
  void OnTaskUpdated(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;

 private:
  void UpdateContextForTask(const base::Uuid& task_id);

  mojo::Receiver<contextual_tasks::mojom::PageHandler> receiver_;
  raw_ptr<ContextualTasksUI> web_ui_controller_;
  raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;
  raw_ptr<contextual_tasks::ContextualTasksContextController>
      context_controller_;

  base::ScopedObservation<contextual_tasks::ContextualTasksService,
                          contextual_tasks::ContextualTasksService::Observer>
      context_controller_observation_{this};

  base::WeakPtrFactory<ContextualTasksPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
