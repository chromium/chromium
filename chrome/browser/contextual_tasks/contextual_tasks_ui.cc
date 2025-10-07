// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/feature_list.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ContextualTasksUI::ContextualTasksUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(), kContextualTasksUiHost);
  webui::SetupWebUIDataSource(source, kContextualTasksResources,
                              IDR_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_HTML);

  // TODO(447633840): This is a placeholder URL until the real page is ready.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' https://google.com;");
}

ContextualTasksUI::~ContextualTasksUI() = default;

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler) {
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      std::move(page), std::move(page_handler), web_ui(), this);
}

void ContextualTasksUI::MaybeShowUi() {
  if (embedder()) {
    embedder()->ShowUI();
  }
}

void ContextualTasksUI::BindInterface(
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandlerFactory>
        pending_receiver) {
  factory_receiver_.reset();
  factory_receiver_.Bind(std::move(pending_receiver));
}

bool ContextualTasksUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks);
}

std::unique_ptr<content::WebUIController>
ContextualTasksUIConfig::CreateWebUIController(content::WebUI* web_ui,
                                               const GURL& url) {
  return std::make_unique<ContextualTasksUI>(web_ui);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)
