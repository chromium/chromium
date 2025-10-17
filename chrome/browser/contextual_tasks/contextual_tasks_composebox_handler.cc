// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "base/notreached.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/composebox_omnibox_client.h"
#include "ui/base/window_open_disposition_utils.h"

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
              /*view=*/nullptr,
              std::make_unique<composebox::ComposeboxOmniboxClient>(
                  profile,
                  web_contents,
                  this))),
      page_{std::move(pending_page)},
      handler_(this, std::move(pending_handler)) {}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() = default;

void ContextualTasksComposeboxHandler::SubmitQuery(
    const std::string& query_text,
    WindowOpenDisposition disposition,
    std::map<std::string, std::string> additional_params) {}

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
