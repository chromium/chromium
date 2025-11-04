// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_omnibox_client.h"
#include "ui/base/window_open_disposition_utils.h"

namespace {

class ContextualTasksOmniboxClient : public SearchboxOmniboxClient {
 public:
  ContextualTasksOmniboxClient(Profile* profile,
                               content::WebContents* web_contents);
  ~ContextualTasksOmniboxClient() override;

  // OmniboxClient:
  metrics::OmniboxEventProto::PageClassification GetPageClassification(
      bool is_prefetch) const override;
};

ContextualTasksOmniboxClient::ContextualTasksOmniboxClient(
    Profile* profile,
    content::WebContents* web_contents)
    : SearchboxOmniboxClient(profile, web_contents) {}

ContextualTasksOmniboxClient::~ContextualTasksOmniboxClient() = default;

metrics::OmniboxEventProto::PageClassification
ContextualTasksOmniboxClient::GetPageClassification(bool is_prefetch) const {
  // TODO (crbug.com/454388407): This page classification should be passed in
  // from the embedder so that it can be customized. Currently, it is logging
  // as NTP_COMPOSEBOX, but it should be its own page classification.
  return metrics::OmniboxEventProto::NTP_COMPOSEBOX;
}

}  // namespace

ContextualTasksComposeboxHandler::ContextualTasksComposeboxHandler(
    Profile* profile,
    content::WebContents* web_contents,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler)
    : SearchboxHandler(
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ContextualTasksOmniboxClient>(profile,
                                                             web_contents))),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {
  autocomplete_controller_observation_.Observe(autocomplete_controller());
}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() = default;

void ContextualTasksComposeboxHandler::NotifySessionStarted() {
  // noop.
}

void ContextualTasksComposeboxHandler::NotifySessionAbandoned() {
  // noop.
}

void ContextualTasksComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {}

void ContextualTasksComposeboxHandler::AddFileContext(
    searchbox::mojom::SelectedFileInfoPtr file_info,
    mojo_base::BigBuffer file_bytes,
    AddFileContextCallback callback) {
  // noop.
}

void ContextualTasksComposeboxHandler::AddTabContext(int32_t tab_id,
                                                     bool delay_upload,
                                                     AddTabContextCallback) {
  // noop.
}

void ContextualTasksComposeboxHandler::DeleteContext(
    const base::UnguessableToken& file_token) {
  // noop.
}

void ContextualTasksComposeboxHandler::ClearFiles() {
  // noop.
}

void ContextualTasksComposeboxHandler::DeleteAutocompleteMatch(
    uint8_t line,
    const GURL& url) {
  // noop.
}

void ContextualTasksComposeboxHandler::ExecuteAction(
    uint8_t line,
    uint8_t action_index,
    const GURL& url,
    base::TimeTicks match_selection_timestamp,
    uint8_t mouse_button,
    bool alt_key,
    bool ctrl_key,
    bool meta_key,
    bool shift_key) {
  // noop.
}

void ContextualTasksComposeboxHandler::OnThumbnailRemoved() {
  // noop.
}

void ContextualTasksComposeboxHandler::FocusChanged(bool focused) {
  // noop.
}

void ContextualTasksComposeboxHandler::SetDeepSearchMode(bool enabled) {
  // Ignore, intentionally unimplemented for Lens. Deep search not implemented
  // in Lens.
}

void ContextualTasksComposeboxHandler::SetCreateImageMode(bool enabled,
                                                          bool image_present) {
  // Ignore, intentionally unimplemented for Lens. Create image not implemented
  // in Lens.
}

void ContextualTasksComposeboxHandler::HandleLensButtonClick() {
  // noop
}
