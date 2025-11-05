// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/webui/webui_util.h"

ContextualTasksUI::ContextualTasksUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui),
      ui_service_(contextual_tasks::ContextualTasksUiServiceFactory::
                      GetForBrowserContext(
                          web_ui->GetWebContents()->GetBrowserContext())),
      context_controller_(
          contextual_tasks::ContextualTasksContextControllerFactory::
              GetForProfile(Profile::FromBrowserContext(
                  web_ui->GetWebContents()->GetBrowserContext()))) {
  inner_web_contents_creation_observer_ =
      std::make_unique<InnerFrameCreationObvserver>(
          web_ui->GetWebContents(),
          base::BindOnce(&ContextualTasksUI::OnInnerWebContentsCreated,
                         weak_ptr_factory_.GetWeakPtr()));
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIContextualTasksHost);
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
  // Enable typed suggest.
  source->AddBoolean("composeboxShowTypedSuggest", true);
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
  source->AddString("searchboxComposePlaceholder", "[i18n] Ask Google...");
  source->AddBoolean("composeboxShowPdfUpload", false);
  source->AddBoolean("composeboxSmartComposeEnabled", false);
  source->AddBoolean("composeboxShowDeepSearchButton", false);
  source->AddBoolean("composeboxShowCreateImageButton", false);
  source->AddBoolean("composeboxShowRecentTabChip", false);
  source->AddBoolean("composeboxShowSubmit", true);
}

ContextualTasksUI::~ContextualTasksUI() = default;

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler) {
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      std::move(page), std::move(page_handler), web_ui(), this, ui_service_);
}

const std::optional<base::Uuid>& ContextualTasksUI::GetTaskId() {
  return task_id_;
}

void ContextualTasksUI::SetTaskId(std::optional<base::Uuid> id) {
  task_id_ = id;
}

const std::optional<std::string>& ContextualTasksUI::GetThreadId() {
  return thread_id_;
}

void ContextualTasksUI::SetThreadId(std::optional<std::string> id) {
  thread_id_ = id;
}

const std::optional<std::string>& ContextualTasksUI::GetThreadTitle() {
  return thread_title_;
}

void ContextualTasksUI::SetThreadTitle(std::optional<std::string> title) {
  thread_title_ = title;
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
  composebox_handler_ = std::make_unique<ContextualTasksComposeboxHandler>(
      Profile::FromWebUI(web_ui()), web_ui()->GetWebContents(),
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_handler));
  composebox_handler_->SetPage(std::move(pending_searchbox_page));
}

void ContextualTasksUI::OnInnerWebContentsCreated(
    content::WebContents* inner_contents) {
  // This should only ever happen once per WebUI.
  CHECK(!nav_observer_);
  nav_observer_ = std::make_unique<FrameNavObserver>(
      inner_contents, ui_service_, context_controller_, this);
  inner_web_contents_creation_observer_.reset();
}

ContextualTasksUI::FrameNavObserver::FrameNavObserver(
    content::WebContents* web_contents,
    contextual_tasks::ContextualTasksUiService* ui_service,
    contextual_tasks::ContextualTasksContextController* context_controller,
    TaskInfoDelegate* task_info_delegate)
    : content::WebContentsObserver(web_contents),
      ui_service_(ui_service),
      context_controller_(context_controller),
      task_info_delegate_(CHECK_DEREF(task_info_delegate)) {}

void ContextualTasksUI::FrameNavObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!ui_service_ || !context_controller_) {
    return;
  }

  // Ignore sub-frame and uncommitted navigations.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // TODO(456245130): Consider making this next part a CHECK since it should be
  //                  impossible for this to not be an AI URL.
  const GURL& url = navigation_handle->GetURL();
  if (!ui_service_->IsAiUrl(url)) {
    return;
  }

  // Almost everything is keyed off of the thread ID - if one isn't in the URL,
  // wait until it is. This state also implies the task and thread we're
  // tracking changed.
  std::string url_thread_id;
  if (!net::GetValueForKeyInQuery(url, "mtid", &url_thread_id)) {
    task_info_delegate_->SetTaskId(std::nullopt);
    task_info_delegate_->SetThreadId(std::nullopt);
    task_info_delegate_->SetThreadTitle(std::nullopt);
    return;
  }

  auto webui_thread_id = task_info_delegate_->GetThreadId();

  // In cases where the webui doesn't know about an existing threaad ID or
  // there's a mismatch, either create a new task or update to use an existing
  // one (if it exists).
  if (!webui_thread_id || (webui_thread_id.value() != url_thread_id)) {
    // Check if there's an existing task for the thread.
    std::optional<contextual_tasks::ContextualTask> existing_task =
        context_controller_->GetTaskFromServerId(
            contextual_tasks::ThreadType::kAiMode, url_thread_id);

    if (existing_task) {
      task_info_delegate_->SetTaskId(existing_task.value().GetTaskId());
      task_info_delegate_->SetThreadTitle(existing_task.value().GetTitle());
    } else {
      auto task = context_controller_->CreateTaskFromUrl(url);
      task_info_delegate_->SetTaskId(task.GetTaskId());
    }
  }
  task_info_delegate_->SetThreadId(url_thread_id);

  // If we don't yet have a title, try to pull one from the query.
  if (!task_info_delegate_->GetThreadTitle()) {
    std::string query_value;
    if (net::GetValueForKeyInQuery(url, "q", &query_value)) {
      task_info_delegate_->SetThreadTitle(query_value);
    }
  }

  std::optional<std::string> mstk;
  mstk.emplace();
  if (!net::GetValueForKeyInQuery(url, "mstk", &mstk.value())) {
    mstk = std::nullopt;
  }

  context_controller_->UpdateThreadForTask(
      task_info_delegate_->GetTaskId().value(),
      contextual_tasks::ThreadType::kAiMode, url_thread_id, mstk,
      task_info_delegate_->GetThreadTitle());
}

ContextualTasksUI::InnerFrameCreationObvserver::InnerFrameCreationObvserver(
    content::WebContents* web_contents,
    base::OnceCallback<void(content::WebContents*)> callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {}

ContextualTasksUI::InnerFrameCreationObvserver::~InnerFrameCreationObvserver() =
    default;

void ContextualTasksUI::InnerFrameCreationObvserver::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  CHECK(callback_);
  std::move(callback_).Run(inner_web_contents);
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)
