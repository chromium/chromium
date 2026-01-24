// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/uuid.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_search_session_finder.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_tasks/public/context_decoration_params.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/contextual_tasks/public/utils.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/l10n/l10n_util.h"
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
  ping->add_capabilities(lens::FeatureCapability::OPEN_THREADS_VIEW);
  ping->add_capabilities(lens::FeatureCapability::COBROWSING_DISPLAY_CONTROL);
  if (base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksContextLibrary)) {
    ping->add_capabilities(lens::FeatureCapability::THREAD_CONTEXT_LIBRARY);
  }
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(&serialized_message[0], size);
  return base::Base64Encode(serialized_message);
}

}  // namespace

void AddDefaultZeroStateStrings(content::WebUIDataSource* source) {
  source->AddString("friendlyZeroStateTitle",
                    l10n_util::GetStringUTF16(
                        IDS_AI_MODE_FRIENDLY_ZERO_STATE_TITLE_WITHOUT_NAME));
  source->AddString("friendlyZeroStateSubtitle", "");
}

void AddZeroStateStrings(content::WebUIDataSource* source, Profile* profile) {
  if (!profile) {
    AddDefaultZeroStateStrings(source);
    return;
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (!entry) {
    AddDefaultZeroStateStrings(source);
    return;
  }

  std::u16string gaia_name = entry->GetGAIANameToDisplay();
  std::u16string full_string;
  if (gaia_name.empty()) {
    full_string = l10n_util::GetStringUTF16(
        IDS_AI_MODE_FRIENDLY_ZERO_STATE_TITLE_WITHOUT_NAME);
  } else {
    full_string = l10n_util::GetStringFUTF16(
        IDS_AI_MODE_FRIENDLY_ZERO_STATE_TITLE, gaia_name, u"<br>");
  }
  std::vector<std::u16string> parts = base::SplitStringUsingSubstr(
      full_string, u"<br>", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  source->AddString("friendlyZeroStateTitle", parts.empty() ? u"" : parts[0]);
  source->AddString("friendlyZeroStateSubtitle",
                    parts.size() > 1 ? parts[1] : u"");
}

ContextualTasksUI::ContextualTasksUI(content::WebUI* web_ui)
    : TopChromeWebUIController(web_ui),
      ui_service_(contextual_tasks::ContextualTasksUiServiceFactory::
                      GetForBrowserContext(
                          web_ui->GetWebContents()->GetBrowserContext())),
      contextual_tasks_service_(
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  web_ui->GetWebContents()->GetBrowserContext()))) {
  inner_web_contents_creation_observer_ =
      std::make_unique<InnerFrameCreationObvserver>(
          web_ui->GetWebContents(),
          base::BindRepeating(&ContextualTasksUI::OnInnerWebContentsCreated,
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
      {"sourcesMenuTitle", IDS_CONTEXTUAL_TASKS_SOURCES_MENU_TITLE},
      {"sourcesMenuTabsHeader", IDS_CONTEXTUAL_TASKS_SOURCES_MENU_TABS_HEADER},
      {"title", IDS_CONTEXTUAL_TASKS_AI_MODE_TITLE},
      /* composeDeepSearchPlaceholder and
       * composeCreateImagePlaceholder are defined by searchbox_handler.cc.
       */
      {"composeboxPlaceholderText", IDS_NTP_COMPOSE_PLACEHOLDER_TEXT},
      {"onboardingTitle", IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_TITLE},
      {"onboardingBody", IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_DESCRIPTION},
      {"onboardingLink", IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_LEARN_MORE},
      {"permissionError", IDS_NEW_TAB_VOICE_PERMISSION_ERROR},
      {"listening", IDS_NEW_TAB_VOICE_LISTENING},
  };
  source->AddLocalizedStrings(kLocalizedStrings);
  source->AddLocalizedString(
      "lensSearchButtonLabel",
      IDS_TOOLTIP_LENS_REINVOKE_VISUAL_SELECTION_A11Y_LABEL);

  source->AddString(
      "onboardingLinkUrl",
      contextual_tasks::GetContextualTasksOnboardingTooltipHelpUrl());
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
  source->AddBoolean("composeboxShowContextMenuDescription", false);
  // Send event when escape is pressed.
  source->AddBoolean("composeboxCloseByEscape", true);
  source->AddBoolean(
      "showOnboardingTooltip",
      base::FeatureList::IsEnabled(
          contextual_tasks::kContextualTasksShowOnboardingTooltip));
  source->AddInteger(
      "composeboxShowOnboardingTooltipSessionImpressionCap",
      contextual_tasks::
          GetContextualTasksShowOnboardingTooltipSessionImpressionCap());
  source->AddInteger(
      "composeboxShowOnboardingTooltipImpressionDelay",
      contextual_tasks::GetContextualTasksOnboardingTooltipImpressionDelay());
  source->AddBoolean(
      "isOnboardingTooltipDismissCountBelowCap",
      Profile::FromWebUI(web_ui)->GetPrefs()->GetInteger(
          contextual_tasks::kContextualTasksOnboardingTooltipDismissedCount) <
          contextual_tasks::GetContextualTasksOnboardingTooltipDismissedCap());
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
  source->AddString("searchboxComposePlaceholder",
                    ntp_composebox::FeatureConfig::Get()
                        .config.composebox()
                        .input_placeholder_text());
  source->AddBoolean("composeboxSmartComposeEnabled", false);
  AddContextMenuItemEligibilityLoadTimeData(source, Profile::FromWebUI(web_ui));
  source->AddBoolean("composeboxShowLensSearchChip", false);
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
  source->AddBoolean("composeboxContextMenuEnableMultiTabSelection", true);
  source->AddBoolean(
      "darkMode", ThemeServiceFactory::GetForProfile(Profile::FromWebUI(web_ui))
                      ->BrowserUsesDarkColors());
  source->AddBoolean("clearAllInputsWhenSubmittingQuery", true);
  source->AddBoolean("autoSubmitVoiceSearchQuery",
                     contextual_tasks::GetAutoSubmitVoiceSearchQuery());

  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kContextualTasks));
  source->AddLocalizedString(
      "protectedErrorPageTopLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_FIRST_LINE);
  source->AddLocalizedString(
      "protectedErrorPageBottomLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_SECOND_LINE);

  source->AddString("userAgentSuffix",
                    contextual_tasks::GetContextualTasksUserAgentSuffix());
  // Preload the serialized handshake message so it doesn't have to be fetched
  // at runtime.
  source->AddString("handshakeMessage", GetEncodedHandshakeMessage());

  // Force a host for any URL opened in the embedded page. If empty, no change
  // is made to the URL.
  source->AddString("forcedEmbeddedPageHost",
                    contextual_tasks::GetForcedEmbeddedPageHost());
  source->AddString(
      "contextualTasksSignInDomains",
      base::JoinString(contextual_tasks::GetContextualTasksSignInDomains(),
                       ","));

  // Set up chrome://contextual-tasks/internals debug UI.
  source->AddResourcePath(
      "internals",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);
  source->AddResourcePath(
      "internals/",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);

  Profile* profile = Profile::FromWebUI(web_ui);
  AddZeroStateStrings(source, profile);
}

ContextualTasksUI::~ContextualTasksUI() = default;

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler) {
  // Reset the page and page handler before binding in case they already exists
  // (like on a reload). Not resetting them can cause unintended behavior.
  page_.reset();
  page_handler_.reset();

  page_.Bind(std::move(page));
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      std::move(page_handler), this, ui_service_, contextual_tasks_service_);

  // Request the initial OAuth token to be used by the embedded page.
  RequestOAuthToken();
}

void ContextualTasksUI::RequestOAuthToken() {
  token_refresh_timer_.Stop();

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
  base::UmaHistogramEnumeration("ContextualTasks.WebUI.OAuthError",
                                error.state(),
                                GoogleServiceAuthError::NUM_STATES);

  oauth_token_fetcher_.reset();
  if (!page_) {
    return;
  }
  if (error.state() != GoogleServiceAuthError::NONE) {
    page_->SetOAuthToken("");
    return;
  }
  page_->SetOAuthToken(access_token_info.token);

  if (!access_token_info.expiration_time.is_null()) {
    token_refresh_timer_.Start(
        FROM_HERE, access_token_info.expiration_time - base::Time::Now(),
        base::BindOnce(&ContextualTasksUI::RequestOAuthToken,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void ContextualTasksUI::OnZeroStateChange(bool is_zero_state) {
  if (page_) {
    page_->OnZeroStateChange(is_zero_state);
  }
}

const std::optional<base::Uuid>& ContextualTasksUI::GetTaskId() {
  return task_id_;
}

void ContextualTasksUI::SetTaskId(std::optional<base::Uuid> id) {
  task_id_ = id;
  PushTaskDetailsToPage();
}

const std::optional<std::string>& ContextualTasksUI::GetThreadId() {
  return thread_id_;
}

void ContextualTasksUI::SetThreadId(std::optional<std::string> id) {
  thread_id_ = id;
  PushTaskDetailsToPage();
}

void ContextualTasksUI::SetThreadTurnId(std::optional<std::string> id) {
  thread_turn_id_ = id;
  PushTaskDetailsToPage();
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

void ContextualTasksUI::SetIsAiPage(bool is_ai_page) {
  if (page_) {
    page_->OnAiPageStatusChanged(is_ai_page);
  }

  // When AI page is first loaded, close the Lens overlay if it's open.
  if (is_ai_page && !was_ai_page_) {
    auto* browser = GetBrowser();
    if (browser) {
      if (auto* controller = LensSearchController::FromTabWebContents(
              browser->GetTabStripModel()->GetActiveWebContents())) {
        controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
      }
    }
  }
  was_ai_page_ = is_ai_page;
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
  auto* coordinator = GetSidePanelCoordinator();
  if (!coordinator) {
    return;
  }

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
  // Check if the user should have landed on the WebUI via an entry point. If
  // not, refuse to load the WebUI to prevent a broken experience.
  return base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
         contextual_tasks::EntryPointEligibilityManager::IsEligible(
             Profile::FromBrowserContext(browser_context));
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
      std::move(pending_searchbox_handler),
      base::BindRepeating(
          &ContextualTasksUI::GetOrCreateContextualSessionHandle,
          base::Unretained(this)));
  composebox_handler_->SetPage(std::move(pending_searchbox_page));
}

contextual_search::ContextualSearchSessionHandle*
ContextualTasksUI::GetOrCreateContextualSessionHandle() {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  // Check if a session exists for the current task.
  contextual_search::ContextualSearchSessionHandle* existing_session =
      task_id_.has_value() ? helper->GetSessionForTask(task_id_.value())
                           : helper->session_handle();
  if (existing_session) {
    return existing_session;
  }

  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(
          Profile::FromWebUI(web_ui()));

  // Create a new session if there's no task ID yet.
  if (!task_id_) {
    if (contextual_search_service) {
      auto session_handle = contextual_search_service->CreateSession(
          ntp_composebox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kContextualTasks);
      // TODO(crbug.com/469875164): Determine what to do with the return value
      // of this call, or move this call to a different location.
      session_handle->CheckSearchContentSharingSettings(
          Profile::FromWebUI(web_ui())->GetPrefs());
      helper->SetTaskSession(std::nullopt, std::move(session_handle));
      return helper->session_handle();
    }
  }

  // If no valid session exists, maintains context continuity by trying to find
  // one from affiliated tabs or side panel WebContents.
  auto* coordinator = GetSidePanelCoordinator();
  if (!coordinator || !task_id_.has_value()) {
    return nullptr;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  UpdateContextualSearchWebContentsHelperForTask(
      contextual_search_service,
      /*browser_window=*/browser_window_interface, contextual_tasks_service_,
      coordinator, web_contents, task_id_.value());
  return helper->session_handle();
}

void ContextualTasksUI::PostMessageToWebview(
    const lens::ClientToAimMessage& message) {
  CHECK(page_handler_);
  page_handler_->PostMessageToWebview(message);
}

void ContextualTasksUI::OnInnerWebContentsCreated(
    content::WebContents* inner_contents) {
  // This can be called multiple times if the page is reloaded.
  nav_observer_ = std::make_unique<FrameNavObserver>(
      inner_contents, ui_service_, contextual_tasks_service_, this);
  embedded_web_contents_ = inner_contents->GetWeakPtr();
}

void ContextualTasksUI::OnContextRetrievedForActiveTab(
    int32_t tab_id,
    const GURL& last_committed_url,
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
  // Do nothing is the webUI is no longer in side panel.
  if (IsShownInTab()) {
    return;
  }

  // If active tab or tab URL changed since the GetContextForTask() call, do
  // nothing.
  tabs::TabInterface* tab = GetBrowser()->GetActiveTabInterface();
  if (!tab || tab->GetHandle().raw_value() != tab_id ||
      tab->GetContents()->GetLastCommittedURL() != last_committed_url) {
    return;
  }

  // If last_committed_url is already in the context, clear the suggested tab
  // context.
  std::unique_ptr<url_deduplication::URLDeduplicationHelper>
      url_duplication_helper =
          contextual_tasks::CreateURLDeduplicationHelperForContextualTask();
  if (context &&
      context->ContainsURL(last_committed_url, url_duplication_helper.get())) {
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
    return;
  }

  UpdateSuggestedTabContext(tab);
}

void ContextualTasksUI::UpdateSuggestedTabContext(tabs::TabInterface* tab) {
  content::WebContents* web_contents = tab->GetContents();
  auto tab_data = searchbox::mojom::TabInfo::New();
  tab_data->tab_id = tab->GetHandle().raw_value();
  tab_data->title = base::UTF16ToUTF8(web_contents->GetTitle());
  tab_data->url = web_contents->GetLastCommittedURL();
  tab_data->last_active = std::max(web_contents->GetLastActiveTimeTicks(),
                                   web_contents->GetLastInteractionTimeTicks());
  composebox_handler_->UpdateSuggestedTabContext(std::move(tab_data));
}

void ContextualTasksUI::OnSidePanelStateChanged() {
  page_->OnSidePanelStateChanged();

  lens::ClientToAimMessage message;
  auto* display_mode_msg = message.mutable_set_cobrowsing_display_mode();
  if (IsShownInTab()) {
    display_mode_msg->mutable_payload()->set_display_mode(
        lens::CobrowsingDisplayModeParams::COBROWSING_TAB);
    if (previous_web_ui_state_ != WebUIState::kShownInTab) {
      previous_web_ui_state_ = WebUIState::kShownInTab;
      if (composebox_handler_) {
        composebox_handler_->UpdateSuggestedTabContext(nullptr);
      }
    }
  } else {
    if (previous_web_ui_state_ != WebUIState::kShownInSidePanel &&
        GetBrowser()) {
      // The WebUI starts showing in the side panel, show the auto suggested
      // chip if possible.
      previous_web_ui_state_ = WebUIState::kShownInSidePanel;
      OnActiveTabContextStatusChanged();
    }
    display_mode_msg->mutable_payload()->set_display_mode(
        lens::CobrowsingDisplayModeParams::COBROWSING_SIDEPANEL);
  }

  PostMessageToWebview(message);
}

void ContextualTasksUI::DisableActiveTabContextSuggestion() {
  ui_service_->set_auto_tab_context_suggestion_enabled(false);

  // Notify the active task context provider that the side panel state has
  // changed.
  auto* browser = webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return;
  }
  auto* active_task_context_provider =
      browser->GetFeatures().contextual_tasks_active_task_context_provider();
  if (active_task_context_provider) {
    active_task_context_provider->RefreshContext();
  }
}

void ContextualTasksUI::OnLensOverlayStateChanged(bool is_showing) {
  if (page_) {
    page_->OnLensOverlayStateChanged(is_showing);
  }
}

void ContextualTasksUI::OnActiveTabContextStatusChanged() {
  if (!GetBrowser()) {
    return;
  }

  if (!composebox_handler_) {
    return;
  }

  if (!ui_service_->auto_tab_context_suggestion_enabled()) {
    return;
  }

  if (contextual_tasks::GetIsProtectedPageErrorEnabled() && page_) {
    page_->HideErrorPage();
  }

  tabs::TabInterface* tab = GetBrowser()->GetActiveTabInterface();
  if (!tab) {
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
    return;
  }

  content::WebContents* web_contents = tab->GetContents();
  GURL last_committed_url = web_contents->GetLastCommittedURL();

  if (!last_committed_url.is_valid() ||
      !last_committed_url.SchemeIsHTTPOrHTTPS()) {
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
    return;
  }

  if (!GetOrCreateContextualSessionHandle()) {
    return;
  }

  auto context_decoration_params =
      std::make_unique<contextual_tasks::ContextDecorationParams>();
  context_decoration_params->contextual_search_session_handle =
      GetOrCreateContextualSessionHandle()->AsWeakPtr();
  contextual_tasks_service_->GetContextForTask(
      GetTaskId().value(),
      {contextual_tasks::ContextualTaskContextSource::kPendingContextDecorator},
      std::move(context_decoration_params),
      base::BindOnce(&ContextualTasksUI::OnContextRetrievedForActiveTab,
                     weak_ptr_factory_.GetWeakPtr(),
                     tab->GetHandle().raw_value(), last_committed_url));
}

void ContextualTasksUI::OnPageContextEligibilityChecked(
    bool is_page_context_eligible) {
  if (!contextual_tasks::GetIsProtectedPageErrorEnabled() || !page_) {
    return;
  }
  if (is_page_context_eligible) {
    page_->HideErrorPage();
  } else {
    page_->ShowErrorPage();
    base::UmaHistogramEnumeration(
        base::StrCat({"ContextualSearch.ErrorPageShown", ".",
                      contextual_search::ContextualSearchMetricsRecorder::
                          ContextualSearchSourceToString(
                              contextual_search::ContextualSearchSource::
                                  kContextualTasks)}),
        contextual_search::ContextualSearchErrorPage::kPageContextNotEligible);
  }
}

void ContextualTasksUI::TransferNavigationToEmbeddedPage(
    content::OpenURLParams params) {
  bool is_allowed_url = ui_service_->IsValidSearchResultsPage(params.url) ||
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

bool ContextualTasksUI::IsActiveTabContextSuggestionShowing() const {
  return composebox_handler_ &&
         composebox_handler_->has_suggested_tab_context();
}

void ContextualTasksUI::PushTaskDetailsToPage() {
  page_->SetTaskDetails(task_id_.value_or(base::Uuid()),
                        thread_id_.value_or(""), thread_turn_id_.value_or(""));
}

contextual_tasks::ContextualTasksSidePanelCoordinator*
ContextualTasksUI::GetSidePanelCoordinator() {
  if (!web_ui()->GetWebContents()) {
    return nullptr;
  }

  auto* browser = webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return nullptr;
  }

  return contextual_tasks::ContextualTasksSidePanelCoordinator::From(browser);
}

ContextualTasksUI::FrameNavObserver::FrameNavObserver(
    content::WebContents* web_contents,
    contextual_tasks::ContextualTasksUiService* ui_service,
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    TaskInfoDelegate* task_info_delegate)
    : content::WebContentsObserver(web_contents),
      ui_service_(ui_service),
      contextual_tasks_service_(contextual_tasks_service),
      task_info_delegate_(CHECK_DEREF(task_info_delegate)) {}

void ContextualTasksUI::FrameNavObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!ui_service_ || !contextual_tasks_service_) {
    return;
  }

  // Ignore sub-frame and uncommitted navigations.
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  auto current_title = task_info_delegate_->GetThreadTitle();

  // Notify the WebUI if the new page is an AI page so it can adjust the UI
  // accordingly.
  const GURL& url = navigation_handle->GetURL();
  bool is_ai_page = ui_service_->IsAiUrl(url);
  task_info_delegate_->SetIsAiPage(is_ai_page);

  // Set whether this navigation is to a zero state so the UI can adjust
  // accordingly.
  const bool is_zero_state = ContextualTasksUI::IsZeroState(url, ui_service_);
  task_info_delegate_->OnZeroStateChange(is_zero_state);

  bool is_url_changed = false;
  if (url != last_committed_url_) {
    last_committed_url_ = url;
    is_url_changed = true;
  }

  if (!is_url_changed) {
    return;
  }

  if (!is_ai_page) {
    return;
  }

  if (is_zero_state) {
    // Create a new task for zero state, since there's no thread to associate
    // this with yet.
    contextual_tasks::ContextualTask task =
        contextual_tasks_service_->CreateTask();
    base::Uuid new_task_id = task.GetTaskId();
    task_info_delegate_->SetTaskId(new_task_id);
    task_info_delegate_->SetThreadId(std::nullopt);
    task_info_delegate_->SetThreadTurnId(std::nullopt);
    task_info_delegate_->SetThreadTitle(std::nullopt);

    ui_service_->OnTaskChanged(task_info_delegate_->GetBrowser(),
                               task_info_delegate_->GetWebUIWebContents(),
                               new_task_id,
                               task_info_delegate_->IsShownInTab());
    task_info_delegate_->OnTaskChanged();
    return;
  }

  std::string query_value;
  if (net::GetValueForKeyInQuery(url, "q", &query_value)) {
    task_info_delegate_->SetThreadTitle(query_value);
  }

  std::string url_thread_id;
  if (!net::GetValueForKeyInQuery(url, "mtid", &url_thread_id)) {
    return;
  }

  auto webui_thread_id = task_info_delegate_->GetThreadId();
  bool task_changed = false;

  // We need to always check if there is an existing task for the thread id.
  std::optional<contextual_tasks::ContextualTask> existing_task =
      contextual_tasks_service_->GetTaskFromServerId(
          contextual_tasks::ThreadType::kAiMode, url_thread_id);

  if (existing_task) {
    // The thread ID belongs to an existing task. We must switch to it, unless
    // we are already on it.
    if (!task_info_delegate_->GetTaskId() ||
        existing_task.value().GetTaskId() != task_info_delegate_->GetTaskId()) {
      task_changed = true;
      task_info_delegate_->SetTaskId(existing_task.value().GetTaskId());
    }
  } else {  // !existing_task
    // The thread ID is new/unknown to the service.
    // We have two sub-cases:
    // 1. We have a "pending" task (created via query, waiting for thread ID).
    //    -> Attach this new ID to the pending task, unless we believe this is
    //       actually a different task (i.e. the title changed).
    // 2. We are on a stable task (already has a thread ID) or no task.
    //    -> This is a brand new conversation. Create a new task.

    bool is_pending_task =
        task_info_delegate_->GetTaskId().has_value() && !webui_thread_id;

    // Check if the title changed while we were in a pending state.
    // We compare `query_value` (new) vs `current_title` (old, captured before
    // processing this navigation. If they differ, we assume the user switched
    // threads while we were in a bad state,  so we must create a NEW task to
    // avoid leaking context.
    bool pending_task_title_mismatch =
        is_pending_task && current_title.has_value() && !query_value.empty() &&
        current_title.value() != query_value;

    // We have no thread ID and no pending task, so this is a fresh start.
    bool is_new_conversation = !webui_thread_id && !is_pending_task;

    // Did we switch from one active thread to another, i.e. we had a thread ID,
    // but the URL has a different one.
    bool is_thread_switch =
        webui_thread_id && webui_thread_id.value() != url_thread_id;

    bool should_create_new_task =
        pending_task_title_mismatch || is_new_conversation || is_thread_switch;

    if (should_create_new_task) {
      task_changed = true;
      auto task = contextual_tasks_service_->CreateTaskFromUrl(url);
      task_info_delegate_->SetTaskId(task.GetTaskId());
    }
  }
  task_info_delegate_->SetThreadId(url_thread_id);

  std::optional<std::string> mstk;
  std::string url_param_mstk;
  if (net::GetValueForKeyInQuery(url, "mstk", &url_param_mstk)) {
    mstk = url_param_mstk;
  }

  contextual_tasks_service_->UpdateThreadForTask(
      task_info_delegate_->GetTaskId().value(),
      contextual_tasks::ThreadType::kAiMode, url_thread_id, mstk,
      task_info_delegate_->GetThreadTitle());
  task_info_delegate_->SetThreadTurnId(mstk);

  if (task_changed) {
    ui_service_->OnTaskChanged(task_info_delegate_->GetBrowser(),
                               task_info_delegate_->GetWebUIWebContents(),
                               task_info_delegate_->GetTaskId().value(),
                               task_info_delegate_->IsShownInTab());
    task_info_delegate_->OnTaskChanged();
  }
}

bool ContextualTasksUI::IsZeroState(
    const GURL& url,
    contextual_tasks::ContextualTasksUiService* ui_service) {
  std::string query_value;
  std::string mstk_value;
  std::string vsrid_value;
  std::string cinpts_value;
  net::GetValueForKeyInQuery(url, "q", &query_value);
  net::GetValueForKeyInQuery(url, "mstk", &mstk_value);
  net::GetValueForKeyInQuery(url, "vsrid", &vsrid_value);
  net::GetValueForKeyInQuery(url, "cinpts", &cinpts_value);

  // If the URL is an AI URL and there's no query or mstk, it's zero state. If
  // there is either a query or mstk, assume it's not zero state. If there is a
  // vsrid/cinpts, assume it's not zero state since there will soon be an mstk.
  // TODO(crbug.com/472336339): Find a more robust way to determine if the page
  // is zero state instead of query params.
  return ui_service->IsAiUrl(url) && query_value.empty() &&
         mstk_value.empty() && vsrid_value.empty() && cinpts_value.empty();
}

ContextualTasksUI::InnerFrameCreationObvserver::InnerFrameCreationObvserver(
    content::WebContents* web_contents,
    base::RepeatingCallback<void(content::WebContents*)> callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)) {}

ContextualTasksUI::InnerFrameCreationObvserver::~InnerFrameCreationObvserver() =
    default;

void ContextualTasksUI::InnerFrameCreationObvserver::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  CHECK(callback_);
  callback_.Run(inner_web_contents);
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
  auto* contextual_tasks_service =
      contextual_tasks::ContextualTasksContextServiceFactory::GetForProfile(
          profile);
  auto* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  contextual_tasks_internals_page_handler_ =
      std::make_unique<ContextualTasksInternalsPageHandler>(
          contextual_tasks_service, optimization_guide_keyed_service,
          std::move(receiver), std::move(page));
}

void ContextualTasksUI::OnTaskChanged() {
  composebox_handler_->OnTaskChanged();
}

// static
base::RefCountedMemory* ContextualTasksUI::GetFaviconResourceBytes(
    ui::ResourceScaleFactor scale_factor) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Use the Google G favicon for Google Chrome branded builds.
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_GOOGLE_G_GRADIENT_16, scale_factor));
#else
  // Use the Chromium favicon for Chromium builds.
  return static_cast<base::RefCountedMemory*>(
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
          IDR_NTP_FAVICON, scale_factor));
#endif
}

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)
