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
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

ContextualTasksInternalsPageHandler::ContextualTasksInternalsPageHandler(
    contextual_tasks::ContextualTasksContextService* context_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    mojo::PendingReceiver<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler>
        receiver,
    mojo::PendingRemote<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPage> page)
    : context_service_(context_service),
      optimization_guide_logger_(
          optimization_guide_keyed_service->GetOptimizationGuideLogger()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->AddObserver(this);
  }
}

ContextualTasksInternalsPageHandler::~ContextualTasksInternalsPageHandler() {
  if (optimization_guide_logger_) {
    optimization_guide_logger_->RemoveObserver(this);
  }
}

void ContextualTasksInternalsPageHandler::GetRelevantContext(
    contextual_tasks_internals::mojom::GetRelevantContextRequestPtr request,
    GetRelevantContextCallback callback) {
  if (!context_service_) {
    std::move(callback).Run(
        contextual_tasks_internals::mojom::GetRelevantContextResponse::New());
    return;
  }

  context_service_->GetRelevantTabsForQuery(
      {
          .tab_selection_mode = request->tab_selection_mode,
          .min_model_score = request->min_model_score,
      },
      request->query,
      /*explicit_urls=*/{},
      base::BindOnce(
          [](GetRelevantContextCallback callback,
             std::vector<content::WebContents*> relevant_tabs) {
            auto result = contextual_tasks_internals::mojom::
                GetRelevantContextResponse::New();
            for (content::WebContents* web_contents : relevant_tabs) {
              auto tab = contextual_tasks_internals::mojom::Tab::New();
              tab->title = base::UTF16ToUTF8(web_contents->GetTitle());
              tab->url = web_contents->GetLastCommittedURL();
              result->relevant_tabs.push_back(std::move(tab));
            }
            std::move(callback).Run(std::move(result));
          },
          std::move(callback)));
}

void ContextualTasksInternalsPageHandler::OnLogMessageAdded(
    base::Time event_time,
    optimization_guide_common::mojom::LogSource log_source,
    const std::string& source_file,
    int source_line,
    const std::string& message) {
  if (page_ && log_source ==
      optimization_guide_common::mojom::LogSource::CONTEXTUAL_TASKS_CONTEXT) {
    page_->OnLogMessageAdded(event_time, source_file, source_line, message);
  }
}
