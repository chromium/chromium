// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include "base/logging.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

ContextualTasksPageHandler::ContextualTasksPageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler,
    content::WebUI* web_ui,
    ContextualTasksUI* web_ui_controller,
    contextual_tasks::ContextualTasksUiService* contextual_tasks_ui_service)
    : page_(std::move(page)),
      page_handler_(this, std::move(page_handler)),
      web_ui_(web_ui),
      web_ui_controller_(web_ui_controller),
      ui_service_(contextual_tasks_ui_service) {}

ContextualTasksPageHandler::~ContextualTasksPageHandler() = default;

void ContextualTasksPageHandler::GetThreadUrl(GetThreadUrlCallback callback) {
  std::move(callback).Run(ui_service_->GetDefaultAiPageUrl());
}

void ContextualTasksPageHandler::ShowUi() {
  web_ui_controller_->MaybeShowUi();
}
