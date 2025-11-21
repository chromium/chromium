// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/webui/searchbox/composebox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_omnibox_client.h"

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
    : ComposeboxHandler(
          std::move(pending_handler),
          std::move(pending_page),
          std::move(pending_searchbox_handler),
          profile,
          web_contents,
          std::make_unique<OmniboxController>(
              std::make_unique<ContextualTasksOmniboxClient>(profile,
                                                             web_contents))) {}

ContextualTasksComposeboxHandler::~ContextualTasksComposeboxHandler() = default;

void ContextualTasksComposeboxHandler::UpdateSuggestedTabContext(
    searchbox::mojom::TabInfoPtr tab_info) {
  // TODO(http://crbug.com/451688545): Push the `tab_info` to ComposeBox UI.
}
