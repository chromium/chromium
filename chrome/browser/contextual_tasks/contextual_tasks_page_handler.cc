// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include "base/check_deref.h"
#include "base/logging.h"
#include "base/uuid.h"
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
      web_ui_(CHECK_DEREF(web_ui)),
      web_ui_controller_(CHECK_DEREF(web_ui_controller)),
      ui_service_(contextual_tasks_ui_service) {}

ContextualTasksPageHandler::~ContextualTasksPageHandler() = default;

void ContextualTasksPageHandler::GetThreadUrl(GetThreadUrlCallback callback) {
  if (ui_service_) {
    std::move(callback).Run(ui_service_->GetDefaultAiPageUrl());
  }
}

void ContextualTasksPageHandler::GetUrlForTask(const base::Uuid& uuid,
                                               GetUrlForTaskCallback callback) {
  if (ui_service_) {
    std::move(callback).Run(ui_service_->GetInitialUrlForTask(uuid));
  }
}

void ContextualTasksPageHandler::SetTaskId(const base::Uuid& uuid) {
  web_ui_controller_->SetTaskId(uuid);
}

void ContextualTasksPageHandler::SetThreadTitle(const std::string& title) {
  web_ui_controller_->SetThreadTitle(title);
}

void ContextualTasksPageHandler::ShowUi() {
  web_ui_controller_->MaybeShowUi();
}
