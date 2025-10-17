// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/feature_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
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
      "child-src 'self' https://*.google.com;");

  // Add required resources for the searchbox.
  SearchboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui));

  // Add strings.js
  source->UseStringsJs();

  // Support no file types.
  source->AddString("composeboxImageFileTypes", "");
  source->AddString("composeboxAttachmentFileTypes", "");
  source->AddInteger("composeboxFileMaxSize", 0);
  source->AddInteger("composeboxFileMaxCount", 0);
  // Disable typed suggest.
  source->AddBoolean("composeboxShowTypedSuggest", false);
  // Disable ZPS.
  source->AddBoolean("composeboxShowZps", false);
  // Disable image context suggestions.
  source->AddBoolean("composeboxShowImageSuggest", false);
  // Disable context menu and related features.
  source->AddBoolean("composeboxShowContextMenu", false);
  source->AddBoolean("composeboxShowContextMenuDescription", true);
  // Send event when escape is pressed.
  source->AddBoolean("composeboxCloseByEscape", true);

  source->AddBoolean("isLensSearchbox", true);
  source->AddBoolean(
      "forceHideEllipsis",
      lens::features::GetVisualSelectionUpdatesHideCsbEllipsis());
  source->AddBoolean(
      "enableCsbMotionTweaks",
      lens::features::GetVisualSelectionUpdatesEnableCsbMotionTweaks());
  source->AddBoolean(
      "enableVisualSelectionUpdates",
      lens::features::IsLensOverlayVisualSelectionUpdatesEnabled());
  source->AddBoolean(
      "enableThumbnailSizingTweaks",
      lens::features::GetVisualSelectionUpdatesEnableThumbnailSizingTweaks());
  source->AddString("searchboxComposePlaceholder", "Placeholder Text");
  source->AddBoolean("composeboxShowPdfUpload", false);
  source->AddBoolean("composeboxSmartComposeEnabled", false);
  source->AddBoolean("composeboxShowDeepSearchButton", false);
  source->AddBoolean("composeboxShowCreateImageButton", false);
}

ContextualTasksUI::~ContextualTasksUI() = default;

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler) {
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      std::move(page), std::move(page_handler), web_ui(), this,
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          web_ui()->GetWebContents()->GetBrowserContext()));
}

void ContextualTasksUI::MaybeShowUi() {
  if (embedder()) {
    embedder()->ShowUI();
  }
}

void ContextualTasksUI::BindInterface(
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandlerFactory>
        pending_receiver) {
  contextual_tasks_page_handler_factory_receiver_.reset();
  contextual_tasks_page_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
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

void ContextualTasksUI::BindInterface(
    mojo::PendingReceiver<composebox::mojom::PageHandlerFactory>
        pending_receiver) {
  composebox_page_handler_factory_receiver_.reset();
  composebox_page_handler_factory_receiver_.Bind(std::move(pending_receiver));
}

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<composebox::mojom::Page> pending_page,
    mojo::PendingReceiver<composebox::mojom::PageHandler> pending_page_handler,
    mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
    mojo::PendingReceiver<searchbox::mojom::PageHandler>
        pending_searchbox_handler) {
  DCHECK(pending_page.is_valid());
  auto handler = std::make_unique<ContextualTasksComposeboxHandler>(
      Profile::FromWebUI(web_ui()), web_ui()->GetWebContents(),
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_handler));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)
