// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/webui/webui_util.h"

namespace {

// A method to add eligibility booleans for context menu items that are shown
// based on AIM eligibility.
void AddContextMenuItemEligibilityLoadTimeData(content::WebUIDataSource* source,
                                               Profile* profile) {
  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  source->AddBoolean("composeboxShowDeepSearchButton",
                     aim_eligibility_service &&
                         aim_eligibility_service->IsDeepSearchEligible());
  source->AddBoolean("composeboxShowCreateImageButton",
                     aim_eligibility_service &&
                         aim_eligibility_service->IsCreateImagesEligible());
  source->AddBoolean("composeboxShowPdfUpload",
                     aim_eligibility_service &&
                         aim_eligibility_service->IsPdfUploadEligible());
}

BrowserWindowInterface* FromWebContents(content::WebContents* web_contents) {
  BrowserWindow* window =
      BrowserWindow::FindBrowserWindowWithWebContents(web_contents);
  if (window) {
    return window->AsBrowserView()->browser();
  }
  return nullptr;
}

std::string GetEncodedHandshakeMessage() {
  lens::ClientToAimMessage message;
  lens::HandshakePing* ping = message.mutable_handshake_ping();
  ping->add_capabilities(lens::FeatureCapability::DEFAULT);
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(&serialized_message[0], size);
  return base::Base64Encode(serialized_message);
}
}  // namespace

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
  SearchboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui),
                                         /*enable_voice_search=*/true,
                                         /*enable_lens_search=*/false);
  // Add strings.js
  source->UseStringsJs();

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"openInNewTab", IDS_CONTEXTUAL_TASKS_MENU_OPEN_IN_NEW_TAB},
      {"myActivity", IDS_CONTEXTUAL_TASKS_MENU_MY_ACTIVITY},
      {"help", IDS_CONTEXTUAL_TASKS_MENU_HELP},
      {"sourcesMenuTabsHeader", IDS_CONTEXTUAL_TASKS_SOURCES_MENU_TABS_HEADER},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
  source->AddLocalizedString(
      "lensSearchButtonLabel",
      IDS_TOOLTIP_LENS_REINVOKE_VISUAL_SELECTION_A11Y_LABEL);

  source->AddString(
      "composeboxImageFileTypes",
      contextual_tasks::kContextualTasksNextboxImageFileTypes.Get());
  source->AddString(
      "composeboxAttachmentFileTypes",
      contextual_tasks::kContextualTasksNextboxAttachmentFileTypes.Get());
  source->AddInteger(
      "composeboxFileMaxSize",
      contextual_tasks::kContextualTasksNextboxMaxFileSize.Get());
  source->AddInteger(
      "composeboxFileMaxCount",
      contextual_tasks::kContextualTasksNextboxMaxFileCount.Get());
  source->AddBoolean("composeboxNoFlickerSuggestionsFix", false);
  // Enable typed suggest.
  source->AddBoolean("composeboxShowTypedSuggest", false);
  source->AddBoolean("composeboxShowTypedSuggestWithContext", false);
  // Disable ZPS.
  source->AddBoolean(
      "composeboxShowZps",
      contextual_tasks::GetIsContextualTasksSuggestionsEnabled());
  // Disable image context suggestions.
  source->AddBoolean(
      "composeboxShowImageSuggest",
      contextual_tasks::GetIsContextualTasksSuggestionsEnabled());
  // Disable context menu and related features.
  source->AddBoolean(
      "composeboxShowContextMenu",
      contextual_tasks::GetIsContextualTasksNextboxContextMenuEnabled());
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
  source->AddString("composeDeepSearchPlaceholder",
                    "[i18n] Search within results...");
  source->AddString("composeCreateImagePlaceholder", "[i18n] Create image...");
  source->AddBoolean("composeboxSmartComposeEnabled", false);
  AddContextMenuItemEligibilityLoadTimeData(source, Profile::FromWebUI(web_ui));
  source->AddBoolean("composeboxShowRecentTabChip", false);
  source->AddBoolean("composeboxShowSubmit", true);
  source->AddBoolean("composeboxContextDragAndDropEnabled", false);
  source->AddBoolean(
      "steadyComposeboxShowVoiceSearch",
      contextual_tasks::GetIsExpandedComposeboxVoiceSearchEnabled());
  source->AddBoolean(
      "expandedComposeboxShowVoiceSearch",
      contextual_tasks::GetIsSteadyComposeboxVoiceSearchEnabled());
  source->AddBoolean("composeboxShowContextMenuTabPreviews", false);
  source->AddBoolean("composeboxContextMenuEnableMultiTabSelection", false);
  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kContextualTasks));

  source->AddString("userAgentSuffix",
                    contextual_tasks::GetContextualTasksUserAgentSuffix());
  // Preload the serialized handshake message so it doesn't have to be fetched
  // at runtime.
  source->AddString("handshakeMessage", GetEncodedHandshakeMessage());

  // Set up chrome://contextual-tasks/internals debug UI.
  source->AddResourcePath(
      "internals",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);
  source->AddResourcePath(
      "internals/",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);

  // Create a session handle on the web contents if it doesn't already exist.
  // TODO(crbug.com/462193737): Pass the session handle from the omnibox
  // or NTP through the web contents helper.
  if (auto* contextual_search_web_contents_helper =
          ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
              web_ui->GetWebContents());
      !contextual_search_web_contents_helper->session_handle()) {
    Profile* profile = Profile::FromWebUI(web_ui);
    auto* contextual_search_service =
        ContextualSearchServiceFactory::GetForProfile(profile);
    auto contextual_session_handle = contextual_search_service->CreateSession(
        ntp_composebox::CreateQueryControllerConfigParams(),
        contextual_search::ContextualSearchSource::kNewTabPage);
    contextual_search_web_contents_helper->set_session_handle(
        std::move(contextual_session_handle));
  }
}

ContextualTasksUI::~ContextualTasksUI() = default;

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler) {
  page_.Bind(std::move(page));
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      std::move(page_handler), this, ui_service_, context_controller_);
  // TODO(crbug.com/461595196): Currently, this grabs the OAuth token once,
  // but it should be refreshed if it expires.
  RequestOAuthToken();
}

void ContextualTasksUI::RequestOAuthToken() {
  auto* profile = Profile::FromWebUI(web_ui());
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  if (!identity_manager ||
      !identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
    if (page_) {
      page_->SetOAuthToken("");
      return;
    }
    return;
  }

  // TODO(crbug.com/461596823): Currently just grabs the primary account, but
  // should use the web identity when available. Additionally, the account
  // should be grabbed once, and used until this WebUI is closed.
  // TODO(crbug.com/462138963): Add error handling for when the account
  // identities fail.
  auto account =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);

  // A previous fetcher for the same owner will be automatically cancelled.
  oauth_token_fetcher_ = identity_manager->CreateAccessTokenFetcherForAccount(
      account.account_id, signin::OAuthConsumerId::kContextualTasks,
      base::BindOnce(&ContextualTasksUI::OnOAuthTokenReceived,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kWaitUntilRefreshTokenAvailable);
}

void ContextualTasksUI::OnOAuthTokenReceived(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  oauth_token_fetcher_.reset();
  if (!page_) {
    return;
  }
  if (error.state() != GoogleServiceAuthError::NONE) {
    page_->SetOAuthToken("");
    return;
  }
  page_->SetOAuthToken(access_token_info.token);
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
  if (page_) {
    page_->SetThreadTitle(thread_title_.value_or(std::string()));
  }
}

const GURL& ContextualTasksUI::GetInnerFrameUrl() const {
  if (!nav_observer_ || !nav_observer_->web_contents()) {
    return GURL::EmptyGURL();
  }

  return nav_observer_->web_contents()->GetLastCommittedURL();
}

bool ContextualTasksUI::IsShownInTab() {
  return tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
}

BrowserWindowInterface* ContextualTasksUI::GetBrowser() {
  return FromWebContents(web_ui()->GetWebContents());
}

content::WebContents* ContextualTasksUI::GetWebUIWebContents() {
  return web_ui()->GetWebContents();
}

void ContextualTasksUI::CloseSidePanel() {
  auto* browser = webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  auto* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser);
  CHECK(coordinator);
  coordinator->Close();
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
  composebox_handler_ = std::make_unique<ContextualTasksComposeboxHandler>(
      this, Profile::FromWebUI(web_ui()), web_ui()->GetWebContents(),
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_handler));
  composebox_handler_->SetPage(std::move(pending_searchbox_page));
}

void ContextualTasksUI::PostMessageToWebview(
    const lens::ClientToAimMessage& message) {
  CHECK(page_handler_);
  page_handler_->PostMessageToWebview(message);
}

void ContextualTasksUI::OnInnerWebContentsCreated(
    content::WebContents* inner_contents) {
  // This should only ever happen once per WebUI.
  CHECK(!nav_observer_);
  nav_observer_ = std::make_unique<FrameNavObserver>(
      inner_contents, ui_service_, context_controller_, this);
  inner_web_contents_creation_observer_.reset();
  embedded_web_contents_ = inner_contents->GetWeakPtr();
}

void ContextualTasksUI::OnSidePanelStateChanged() {
  page_->OnSidePanelStateChanged();
}

void ContextualTasksUI::DisableActiveTabContextSuggestion() {
  ui_service_->set_auto_tab_context_suggestion_enabled(false);
}

void ContextualTasksUI::OnActiveTabContextStatusChanged(
    TabContextStatus status) {
  if (!composebox_handler_) {
    return;
  }

  if (!ui_service_->auto_tab_context_suggestion_enabled()) {
    return;
  }

  if (status != TabContextStatus::kNotUploaded) {
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
    return;
  }

  tabs::TabInterface* tab = GetBrowser()->GetActiveTabInterface();
  if (!tab) {
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
    return;
  }

  content::WebContents* web_contents = tab->GetContents();
  GURL last_committed_url = web_contents->GetLastCommittedURL();

  if (!last_committed_url.is_valid() || last_committed_url.is_empty()) {
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
    return;
  }

  auto tab_data = searchbox::mojom::TabInfo::New();
  tab_data->tab_id = tab->GetHandle().raw_value();
  tab_data->title = base::UTF16ToUTF8(web_contents->GetTitle());
  tab_data->url = last_committed_url;
  tab_data->last_active = std::max(web_contents->GetLastActiveTimeTicks(),
                                   web_contents->GetLastInteractionTimeTicks());
  composebox_handler_->UpdateSuggestedTabContext(std::move(tab_data));
}

void ContextualTasksUI::TransferNavigationToEmbeddedPage(
    content::OpenURLParams params) {
  bool is_allowed_url = ui_service_->IsSearchResultsPage(params.url) ||
                        ui_service_->IsAiUrl(params.url);
  if (!embedded_web_contents_ || !is_allowed_url) {
    return;
  }

  // TODO(465498890): Consider clearning source_site_instance in this case
  //                  since the navigation may be targeting a different storage
  //                  partition.
  params.frame_tree_node_id =
      embedded_web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  embedded_web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
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

  std::string url_thread_id;
  if (!net::GetValueForKeyInQuery(url, "mtid", &url_thread_id)) {
    return;
  }

  auto webui_thread_id = task_info_delegate_->GetThreadId();
  bool task_changed = false;

  // Avoid creating a new task if there's a task ID without a thread ID.
  bool is_pending_task =
      task_info_delegate_->GetTaskId().has_value() && !webui_thread_id;

  // In cases where the webui doesn't know about an existing threaad ID or
  // there's a mismatch, either create a new task or update to use an existing
  // one (if it exists).
  if (!is_pending_task &&
      (!webui_thread_id || (webui_thread_id.value() != url_thread_id))) {
    // Check if there's an existing task for the thread.
    std::optional<contextual_tasks::ContextualTask> existing_task =
        context_controller_->GetTaskFromServerId(
            contextual_tasks::ThreadType::kAiMode, url_thread_id);

    if (existing_task) {
      task_changed =
          task_info_delegate_->GetTaskId() &&
          existing_task.value().GetTaskId() == task_info_delegate_->GetTaskId();
      task_info_delegate_->SetTaskId(existing_task.value().GetTaskId());
      task_info_delegate_->SetThreadTitle(existing_task.value().GetTitle());
    } else {
      task_changed = true;
      auto task = context_controller_->CreateTaskFromUrl(url);
      task_info_delegate_->SetTaskId(task.GetTaskId());
    }
  }
  task_info_delegate_->SetThreadId(url_thread_id);

  // TODO(crbug.com/456793138): Update the contextual search session handle on
  // the webcontents, reusing sessions if the thread already has a corresponding
  // session entry, based on the mtid, once mtid is reliably set by the server.

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

  if (task_changed && !task_info_delegate_->IsShownInTab() &&
      task_info_delegate_->GetBrowser()) {
    ui_service_->OnTaskChangedInPanel(
        task_info_delegate_->GetBrowser(),
        task_info_delegate_->GetWebUIWebContents(),
        task_info_delegate_->GetTaskId().value());
  }
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

void ContextualTasksUI::BindInterface(
    mojo::PendingReceiver<contextual_tasks_internals::mojom::
                              ContextualTasksInternalsPageHandlerFactory>
        receiver) {
  contextual_tasks_internals_page_handler_receiver_.reset();
  contextual_tasks_internals_page_handler_receiver_.Bind(std::move(receiver));
}

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPage> page,
    mojo::PendingReceiver<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler>
        receiver) {
  Profile* profile = Profile::FromWebUI(web_ui());
  auto* context_service =
      contextual_tasks::ContextualTasksContextServiceFactory::GetForProfile(
          profile);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  contextual_tasks_internals_page_handler_ =
      std::make_unique<ContextualTasksInternalsPageHandler>(
          context_service, optimization_guide_keyed_service,
          std::move(receiver), std::move(page));
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)
