// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_ui.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
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

}  // namespace

ContextualTasksPageHandler::ContextualTasksPageHandler(
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> receiver,
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    ContextualTasksUI* web_ui_controller,
    contextual_tasks::ContextualTasksUiService* ui_service)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      web_ui_controller_(web_ui_controller),
      ui_service_(ui_service) {}

ContextualTasksPageHandler::~ContextualTasksPageHandler() = default;

void ContextualTasksPageHandler::GetThreadUrl(GetThreadUrlCallback callback) {
  std::move(callback).Run(ui_service_->GetDefaultAiPageUrl());
}

void ContextualTasksPageHandler::GetUrlForTask(const base::Uuid& uuid,
                                               GetUrlForTaskCallback callback) {
  std::move(callback).Run(ui_service_->GetInitialUrlForTask(uuid));
}

void ContextualTasksPageHandler::SetTaskId(const base::Uuid& uuid) {
  web_ui_controller_->SetTaskId(uuid);
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

void ContextualTasksPageHandler::MoveTaskUiToToNewTab() {
  auto* browser = web_ui_controller_->GetBrowser();
  const auto& task_id = web_ui_controller_->GetTaskId();
  if (!task_id.has_value()) {
    LOG(ERROR) << "Attempted to open in new tab with no valid task ID.";
    return;
  }
  ui_service_->MoveTaskUiToToNewTab(task_id.value(), browser);
}

void ContextualTasksPageHandler::GetOAuthToken(GetOAuthTokenCallback callback) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(
      Profile::FromWebUI(web_ui_controller_->web_ui()));

  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    std::move(callback).Run("");
    return;
  }

  // TODO(crbug.com/461596823): Currently just grabs the primary account, but
  // should use the web identity when available. Additionally, the account
  // should be grabbed once, and used until this WebUI is closed.
  // TODO(crbug.com/462138963): Add error handling for when the account
  // identities fail.
  auto account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  // A previous fetcher for the same owner will be automatically cancelled.
  oauth_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account.account_id, signin::OAuthConsumerId::kContextualTasks,
      base::BindOnce(&ContextualTasksPageHandler::OnOAuthTokenReceived,
                     base::Unretained(this), std::move(callback)),
      signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void ContextualTasksPageHandler::OnOAuthTokenReceived(
    GetOAuthTokenCallback callback,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth_token_fetcher_.reset();
  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(callback).Run("");
    return;
  }
  std::move(callback).Run(access_token_info.token);
}

void ContextualTasksPageHandler::GetAttachedTabs(
    GetAttachedTabsCallback callback) {
  std::vector<contextual_tasks::mojom::TabPtr> tabs;
  // TODO(crbug.com/460614856): Query backend for attached tabs.
  std::move(callback).Run(std::move(tabs));
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
    page_->OnHandshakeComplete();
  }
}

void ContextualTasksPageHandler::GetHandshakeMessage(
    GetHandshakeMessageCallback callback) {
  lens::ClientToAimMessage message;
  lens::HandshakePing* ping = message.mutable_handshake_ping();
  ping->add_capabilities(lens::FeatureCapability::DEFAULT);
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(&serialized_message[0], size);
  std::move(callback).Run(serialized_message);
}

void ContextualTasksPageHandler::PostMessageToWebview(
    const lens::ClientToAimMessage& message) {
  DCHECK(page_);
  if (!page_) {
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

  page_->PostMessageToWebview(serialized_message);
}
