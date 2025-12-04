// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "url/gurl.h"

namespace {

constexpr char kMyActivityUrl[] = "https://myactivity.google.com/myactivity";
constexpr char kHelpUrl[] = "https://support.google.com/websearch/";

void OpenUrlInNewTab(content::WebUI* web_ui, const GURL& url) {
  NavigateParams params(Profile::FromWebUI(web_ui), url,
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
}

std::vector<contextual_tasks::mojom::TabPtr> TabsFromContext(
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
  std::vector<contextual_tasks::mojom::TabPtr> tabs;

  for (const auto& attachment : context->GetUrlAttachments()) {
    auto tab = contextual_tasks::mojom::Tab::New();
    tab->tab_id = attachment.GetTabSessionId().id();
    tab->title = base::UTF16ToUTF8(attachment.GetTitle());
    tab->url = attachment.GetURL();
    tabs.push_back(std::move(tab));
  }

  return tabs;
}

}  // namespace

ContextualTasksPageHandler::ContextualTasksPageHandler(
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> receiver,
    ContextualTasksUI* web_ui_controller,
    contextual_tasks::ContextualTasksUiService* ui_service,
    contextual_tasks::ContextualTasksContextController* context_controller)
    : receiver_(this, std::move(receiver)),
      web_ui_controller_(web_ui_controller),
      ui_service_(ui_service),
      context_controller_(context_controller) {
  CHECK(context_controller_);
  context_controller_observation_.Observe(context_controller_);
}

ContextualTasksPageHandler::~ContextualTasksPageHandler() = default;

void ContextualTasksPageHandler::GetThreadUrl(GetThreadUrlCallback callback) {
  std::move(callback).Run(ui_service_->GetDefaultAiPageUrl());
}

void ContextualTasksPageHandler::GetUrlForTask(const base::Uuid& uuid,
                                               GetUrlForTaskCallback callback) {
  // First check if there's an initial URL.
  std::optional<GURL> initial_url = ui_service_->GetInitialUrlForTask(uuid);
  if (initial_url) {
    std::move(callback).Run(initial_url.value());
    return;
  }

  // There's a slight difference in the callback signature between the mojo
  // api (wants a reference) and the ui service (provided a moved object).
  // The latter can't provide a reference since we're not keeping it
  // long-term, hence wrapping this in a base::BindOnce.
  ui_service_->GetThreadUrlFromTaskId(
      uuid, base::BindOnce([](GetUrlForTaskCallback callback,
                              GURL url) { std::move(callback).Run(url); },
                           std::move(callback)));
}

void ContextualTasksPageHandler::SetTaskId(const base::Uuid& uuid) {
  web_ui_controller_->SetTaskId(uuid);

  // Trigger an update to the UI with the initial set of tabs for this task.
  UpdateContextForTask(uuid);
}

void ContextualTasksPageHandler::SetThreadTitle(const std::string& title) {
  web_ui_controller_->SetThreadTitle(title);
}

void ContextualTasksPageHandler::CloseSidePanel() {
  web_ui_controller_->CloseSidePanel();
}

void ContextualTasksPageHandler::ShowThreadHistory(
    ShowThreadHistoryCallback callback) {
  std::vector<contextual_tasks::mojom::ThreadPtr> threads;
  // TODO(crbug.com/445469925): Query backend asynchronously to get thread
  // history.
  std::move(callback).Run(std::move(threads));
}

void ContextualTasksPageHandler::IsShownInTab(IsShownInTabCallback callback) {
  std::move(callback).Run(web_ui_controller_->IsShownInTab());
}

void ContextualTasksPageHandler::OpenMyActivityUi() {
  OpenUrlInNewTab(web_ui_controller_->web_ui(), GURL(kMyActivityUrl));
}

void ContextualTasksPageHandler::OpenHelpUi() {
  OpenUrlInNewTab(web_ui_controller_->web_ui(), GURL(kHelpUrl));
}

void ContextualTasksPageHandler::MoveTaskUiToNewTab() {
  auto* browser = web_ui_controller_->GetBrowser();
  const auto& task_id = web_ui_controller_->GetTaskId();
  if (!task_id.has_value()) {
    LOG(ERROR) << "Attempted to open in new tab with no valid task ID.";
    return;
  }

  ui_service_->MoveTaskUiToNewTab(task_id.value(), browser,
                                  web_ui_controller_->GetInnerFrameUrl());
}

void ContextualTasksPageHandler::OnTabClickedFromSourcesMenu(int32_t tab_id,
                                                             const GURL& url) {
  if (ui_service_) {
    ui_service_->OnTabClickedFromSourcesMenu(
        tab_id, url,
        webui::GetBrowserWindowInterface(
            web_ui_controller_->web_ui()->GetWebContents()));
  }
}

void ContextualTasksPageHandler::OnWebviewMessage(
    const std::vector<uint8_t>& message) {
  lens::AimToClientMessage aim_to_client_message;
  if (!aim_to_client_message.ParseFromArray(message.data(), message.size())) {
    return;
  }

  if (aim_to_client_message.has_handshake_response()) {
    web_ui_controller_->page()->OnHandshakeComplete();
  }
}

void ContextualTasksPageHandler::PostMessageToWebview(
    const lens::ClientToAimMessage& message) {
  DCHECK(web_ui_controller_->page());
  if (!web_ui_controller_->page()) {
    return;
  }

  const size_t size = message.ByteSizeLong();
  if (size == 0) {
    LOG(WARNING) << "PostMessageToWebview called with an empty message.";
    return;
  }
  std::vector<uint8_t> serialized_message(size);
  if (!message.SerializeToArray(&serialized_message[0], size)) {
    LOG(ERROR) << "Failed to serialize ClientToAimMessage.";
    return;
  }

  web_ui_controller_->page()->PostMessageToWebview(serialized_message);
}

void ContextualTasksPageHandler::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  if (!web_ui_controller_->page()) {
    return;
  }

  const auto& current_task_id = web_ui_controller_->GetTaskId();
  if (current_task_id != task.GetTaskId()) {
    return;
  }

  UpdateContextForTask(task.GetTaskId());
}

void ContextualTasksPageHandler::UpdateContextForTask(
    const base::Uuid& task_id) {
  context_controller_->GetContextForTask(
      task_id, {contextual_tasks::ContextualTaskContextSource::kTabStrip},
      std::make_unique<contextual_tasks::ContextDecorationParams>(),
      base::BindOnce(
          [](base::WeakPtr<ContextualTasksPageHandler> self,
             std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
            if (self && self->web_ui_controller_->page()) {
              self->web_ui_controller_->page()->OnContextUpdated(
                  TabsFromContext(std::move(context)));
            }
          },
          weak_ptr_factory_.GetWeakPtr()));
}
