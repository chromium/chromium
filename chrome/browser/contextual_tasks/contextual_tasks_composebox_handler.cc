// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_omnibox_client.h"
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
    return metrics::OmniboxEventProto::CO_BROWSING_COMPOSEBOX;
  }

  std::optional<lens::proto::LensOverlaySuggestInputs>
  GetLensOverlaySuggestInputs() const override;

  void OnAutocompleteAccept(
      const GURL& destination_url,
      TemplateURLRef::PostContent* post_content,
      WindowOpenDisposition disposition,
      ui::PageTransition transition,
      AutocompleteMatchType::Type match_type,
      base::TimeTicks match_selection_timestamp,
      bool destination_url_entered_without_scheme,
      bool destination_url_entered_with_http_scheme,
      const std::u16string& text,
      const AutocompleteMatch& match,
      const AutocompleteMatch& alternative_nav_match) override;

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

void ContextualTasksOmniboxClient::OnAutocompleteAccept(
    const GURL& destination_url,
    TemplateURLRef::PostContent* post_content,
    WindowOpenDisposition disposition,
    ui::PageTransition transition,
    AutocompleteMatchType::Type match_type,
    base::TimeTicks match_selection_timestamp,
    bool destination_url_entered_without_scheme,
    bool destination_url_entered_with_http_scheme,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const AutocompleteMatch& alternative_nav_match) {
  std::string query_text;
  net::GetValueForKeyInQuery(destination_url, "q", &query_text);
  composebox_handler_->CreateAndSendQueryMessage(query_text);
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
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ContextualTasksOmniboxClient>(profile,
                                                             web_contents,
                                                             this))),
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
