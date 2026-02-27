// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_interface.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace base {
class Uuid;
}

namespace lens {
class InputPlateParametersRequest;
}

namespace contextual_tasks {
class ContextualTasksService;
class ContextualTasksUiService;
mojom::ComposeboxPositionPtr InputPlateConfigToMojo(
    const lens::InputPlateParametersRequest& update_msg);
}  // namespace contextual_tasks

class ContextualTasksPageHandler
    : public contextual_tasks::mojom::PageHandler,
      public contextual_tasks::ContextualTasksService::Observer {
 public:
  ContextualTasksPageHandler(
      mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> receiver,
      contextual_tasks::ContextualTasksUIInterface* web_ui_controller,
      contextual_tasks::ContextualTasksUiService* ui_service,
      contextual_tasks::ContextualTasksService* contextual_tasks_service);
  ~ContextualTasksPageHandler() override;

  // contextual_tasks::mojom::PageHandler:
  void GetThreadUrl(GetThreadUrlCallback callback) override;
  void GetUrlForTask(const base::Uuid& uuid,
                     GetUrlForTaskCallback callback) override;
  void SetTaskId(const base::Uuid& uuid) override;
  void SetThreadTitle(const std::string& title) override;
  void IsZeroState(const GURL& url, IsZeroStateCallback callback) override;
  void IsAiPage(const GURL& url, IsAiPageCallback callback) override;
  void IsPendingErrorPage(const base::Uuid& task_id,
                          IsPendingErrorPageCallback callback) override;
  void CloseSidePanel() override;
  void ShowThreadHistory() override;
  void IsShownInTab(IsShownInTabCallback callback) override;
  void OpenMyActivityUi() override;
  void OpenHelpUi() override;
  void OpenOnboardingHelpUi() override;
  void OpenUrl(const GURL& url, WindowOpenDisposition disposition) override;
  void MoveTaskUiToNewTab() override;
  void OnTabClickedFromSourcesMenu(int32_t tab_id, const GURL& url) override;
  void OnFileClickedFromSourcesMenu(const GURL& url) override;
  void OnImageClickedFromSourcesMenu(const GURL& url) override;
  void OnWebviewMessage(const std::vector<uint8_t>& message) override;
  void GetCommonSearchParams(bool is_dark_mode,
                             bool is_side_panel,
                             GetCommonSearchParamsCallback callback) override;
  void OnboardingTooltipDismissed() override;
  void PostMessageToWebview(const lens::ClientToAimMessage& message);

  // contextual_tasks::ContextualTasksService::Observer:
  void OnTaskAdded(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;
  void OnTaskUpdated(
      const contextual_tasks::ContextualTask& task,
      contextual_tasks::ContextualTasksService::TriggerSource source) override;

  void set_skip_feedback_ui_for_testing(bool skip) {
    skip_feedback_ui_for_testing_ = skip;
  }

 private:
  void UpdateContextForTask(const base::Uuid& task_id);
  void OnReceivedUpdatedThreadContextLibrary(
      const lens::UpdateThreadContextLibrary& message);
  void OnReceivedInjectInput(std::unique_ptr<lens::ModalityChipProps> modality);
  void OnReceivedRemoveInjectedInput(const std::string& id);

  mojo::Receiver<contextual_tasks::mojom::PageHandler> receiver_;
  raw_ptr<contextual_tasks::ContextualTasksUIInterface> web_ui_controller_;
  raw_ptr<contextual_tasks::ContextualTasksUiService> ui_service_;
  raw_ptr<contextual_tasks::ContextualTasksService> contextual_tasks_service_;

  bool skip_feedback_ui_for_testing_ = false;

  base::ScopedObservation<contextual_tasks::ContextualTasksService,
                          contextual_tasks::ContextualTasksService::Observer>
      contextual_tasks_service_observation_{this};

  base::WeakPtrFactory<ContextualTasksPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PAGE_HANDLER_H_
