// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
#include "chrome/browser/ui/webui/searchbox/composebox_handler.h"
#include "components/contextual_tasks/public/features.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace {

class ContextualTasksOmniboxClient : public ContextualOmniboxClient {
 public:
  ContextualTasksOmniboxClient(
      Profile* profile,
      content::WebContents* web_contents,
      ContextualTasksComposeboxHandler* composebox_handler)
      : ContextualOmniboxClient(profile, web_contents),
        composebox_handler_(composebox_handler) {}
  ~ContextualTasksOmniboxClient() override = default;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override {
    // TODO(crbug.com/461890985): We should use something contextual-tasks
    // specific rather than reuse the Lens one.
    return metrics::OmniboxEventProto::LENS_SIDE_PANEL_COMPOSEBOX;
  }

  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

 private:
  raw_ptr<ContextualTasksComposeboxHandler> composebox_handler_;
  raw_ptr<ContextualSearchboxHandler> contextual_searchbox_handler_;
};

std::optional<lens::proto::LensOverlaySuggestInputs>
ContextualTasksOmniboxClient::GetLensOverlaySuggestInputs() const {
  if (!contextual_tasks::GetIsContextualTasksSuggestionsEnabled()) {
    return lens::proto::LensOverlaySuggestInputs();
  }

  return SearchboxOmniboxClient::GetLensOverlaySuggestInputs();
}

}  // namespace

ContextualTasksComposeboxHandler::ContextualTasksComposeboxHandler(
    ContextualTasksUI* ui_controller,
    Profile* profile,
    content::WebContents* web_contents,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler)
    : ComposeboxHandler(std::move(pending_handler),
                        std::move(pending_page),
                        std::move(pending_searchbox_handler),
                        profile,
                        web_contents),
      web_ui_controller_(ui_controller) {}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() = default;

void ContextualTasksComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  CreateAndSendQueryMessage(query_text);
}

void ContextualTasksComposeboxHandler::OpenAutocompleteMatch(
    uint8_t line,
    const GURL& url,
    bool are_matches_showing,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  std::string query_text;
  net::GetValueForKeyInQuery(url, "q", &query_text);
  SubmitQuery(query_text, mouse_button, alt_key, ctrl_key, meta_key, shift_key);
}

void ContextualTasksComposeboxHandler::CreateAndSendQueryMessage(
    const std::string& query) {
  // Create a client to aim message and send it to the page.
  if (auto* contextual_search_web_contents_helper =
          ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
              web_ui_controller_->GetWebUIWebContents());
      contextual_search_web_contents_helper->session_handle()) {
    auto create_client_to_aim_request_info =
        std::make_unique<contextual_search::ContextualSearchContextController::
                             CreateClientToAimRequestInfo>();
    create_client_to_aim_request_info->query_text = query;
    create_client_to_aim_request_info->query_text_source =
        lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT;
    create_client_to_aim_request_info->query_start_time = base::Time::Now();
    lens::ClientToAimMessage client_to_page_message =
        contextual_search_web_contents_helper->session_handle()
            ->CreateClientToAimRequest(
                std::move(create_client_to_aim_request_info));
    web_ui_controller_->PostMessageToWebview(client_to_page_message);
  }
}

void ContextualTasksComposeboxHandler::HandleLensButtonClick() {
  // TODO(crbug.com/461911729): Implement when overlay is ready to integrate.
}
