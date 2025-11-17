// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

ContextualTasksInternalsPageHandler::ContextualTasksInternalsPageHandler(
    contextual_tasks::ContextualTasksContextService* context_service,
    mojo::PendingReceiver<
        contextual_tasks::mojom::ContextualTasksInternalsPageHandler> receiver)
    : context_service_(context_service), receiver_(this, std::move(receiver)) {}

ContextualTasksInternalsPageHandler::~ContextualTasksInternalsPageHandler() =
    default;

void ContextualTasksInternalsPageHandler::GetRelevantContext(
    contextual_tasks::mojom::GetRelevantContextRequestPtr request,
    GetRelevantContextCallback callback) {
  if (!context_service_) {
    std::move(callback).Run(
        contextual_tasks::mojom::GetRelevantContextResponse::New());
    return;
  }

  context_service_->GetRelevantTabsForQuery(
      {
          .tab_selection_mode = request->tab_selection_mode,
      },
      request->query,
      /*explicit_urls=*/{},
      base::BindOnce(
          [](GetRelevantContextCallback callback,
             std::vector<content::WebContents*> relevant_tabs) {
            auto result =
                contextual_tasks::mojom::GetRelevantContextResponse::New();
            for (content::WebContents* web_contents : relevant_tabs) {
              auto tab = contextual_tasks::mojom::Tab::New();
              tab->title = base::UTF16ToUTF8(web_contents->GetTitle());
              tab->url = web_contents->GetLastCommittedURL();
              result->relevant_tabs.push_back(std::move(tab));
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}
