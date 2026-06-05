// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/base64.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ref.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "base/uuid.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/active_task_context_provider.h"
#include "chrome/browser/contextual_tasks/contextual_search_session_finder.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_auto_suggestion_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler_interface.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/plural_string_handler.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
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
#include "components/favicon_base/favicon_url_parser.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/omnibox/common/logger.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "components/url_deduplication/url_deduplication_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_features.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/backoff_entry.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_provider_key.h"
#include "ui/webui/buildflags.h"
#include "ui/webui/webui_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_help_bubble.h"
#include "components/omnibox/browser/searchbox.mojom-forward.h"
#include "components/zoom/zoom_controller.h"  // nogncheck
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#endif

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/grit/guest_view_shared_resources_map.h"  // nogncheck
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)

#if BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX)
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/sanitized_image/sanitized_image_source.h"
#endif

namespace {

// A method to add eligibility booleans for context menu items that are shown
// based on AIM eligibility.
void AddContextMenuItemEligibilityLoadTimeData(content::WebUIDataSource* source,
                                               Profile* profile) {
  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  bool is_aim_eligible =
      aim_eligibility_service && aim_eligibility_service->IsAimEligible();
  source->AddBoolean("isAimEligible", is_aim_eligible);

  if (aim_eligibility_service &&
      aim_eligibility_service->GetSearchboxConfig()->has_hint_text()) {
    source->AddString(
        "searchboxComposePlaceholder",
        aim_eligibility_service->GetSearchboxConfig()->hint_text());
  } else {
    source->AddLocalizedString(
        "searchboxComposePlaceholder",
        IDS_CONTEXTUAL_TASKS_COMPOSEBOX_PLACEHOLDER_TEXT);
  }
}

BrowserWindowInterface* FromWebContents(content::WebContents* web_contents) {
  return webui::GetBrowserWindowInterface(web_contents);
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
  if (base::FeatureList::IsEnabled(
          contextual_tasks::kEnableNotifyZeroStateRenderedCapability)) {
    ping->add_capabilities(lens::FeatureCapability::NOTIFY_ZERO_STATE_RENDERED);
  }
  if (contextual_tasks::ShouldEnableLockAndUnlockInputCapability()) {
    ping->add_capabilities(lens::FeatureCapability::UNLOCK_INPUT);
    ping->add_capabilities(lens::FeatureCapability::LOCK_INPUT);
  }

  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(&serialized_message[0], size);
  return base::Base64Encode(serialized_message);
}

void UpdateDarkModePreferenceFromUrl(content::WebContents* wc,
                                     const GURL& url) {
  std::optional<bool> is_dark_mode = contextual_tasks::GetDarkModeFromUrl(url);
  if (is_dark_mode.has_value()) {
    blink::web_pref::WebPreferences prefs = wc->GetOrCreateWebPreferences();
    prefs.preferred_color_scheme =
        is_dark_mode.value() ? blink::mojom::PreferredColorScheme::kDark
                             : blink::mojom::PreferredColorScheme::kLight;
    wc->SetWebPreferences(prefs);
  } else {
    blink::web_pref::WebPreferences prefs = wc->GetOrCreateWebPreferences();
    ui::ColorProviderKey::ColorMode browser_color_scheme = wc->GetColorMode();
    prefs.preferred_color_scheme =
        browser_color_scheme == ui::ColorProviderKey::ColorMode::kLight
            ? blink::mojom::PreferredColorScheme::kLight
            : blink::mojom::PreferredColorScheme::kDark;
    wc->SetWebPreferences(prefs);
  }
}
}  // namespace

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(ContextualTasksUI,
                                      kSmartTabSharingMenuItemElementId);

void AddDefaultZeroStateStrings(content::WebUIDataSource* source) {
  source->AddString("friendlyZeroStateTitle",
                    l10n_util::GetStringUTF16(
                        IDS_AI_MODE_FRIENDLY_ZERO_STATE_TITLE_WITHOUT_NAME));
  source->AddString("friendlyZeroStateSubtitle", "");
  source->AddString("friendlyZeroStateGaiaName", "");
  source->AddString("friendlyZeroStateTitleBeforeName", "");
  source->AddString("friendlyZeroStateTitleAfterName", "");
}

bool ContextualTasksUI::AreUrlsEqual(const GURL& a,
                                     const GURL& b) {
  if (a == b) {
    return true;
  }

  if (a.host() != b.host()) {
    return false;
  }

  GURL::Replacements replacements;
  replacements.ClearQuery();
  if (a.ReplaceComponents(replacements) != b.ReplaceComponents(replacements)) {
    return false;
  }

  std::vector<std::pair<std::string_view, std::string_view>> a_params;
  for (net::QueryIterator it(a); !it.IsAtEnd(); it.Advance()) {
    a_params.emplace_back(it.GetKey(), it.GetValue());
  }

  std::vector<std::pair<std::string_view, std::string_view>> b_params;
  for (net::QueryIterator it(b); !it.IsAtEnd(); it.Advance()) {
    b_params.emplace_back(it.GetKey(), it.GetValue());
  }

  if (a_params.size() != b_params.size()) {
    return false;
  }

  std::sort(a_params.begin(), a_params.end());
  std::sort(b_params.begin(), b_params.end());

  return a_params == b_params;
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
  if (gaia_name.empty()) {
    AddDefaultZeroStateStrings(source);
    return;
  }

  std::vector<size_t> offsets;
  const std::u16string full_string = l10n_util::GetStringFUTF16(
      IDS_AI_MODE_FRIENDLY_ZERO_STATE_TITLE, {gaia_name}, &offsets);
  std::vector<std::u16string> parts = base::SplitStringUsingSubstr(
      full_string, u"<br>", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  source->AddString("friendlyZeroStateTitle", parts.empty() ? u"" : parts[0]);

  if (offsets.size() == 1 && !parts.empty() &&
      offsets[0] + gaia_name.length() <= parts[0].length()) {
    source->AddString("friendlyZeroStateGaiaName", gaia_name);
    source->AddString("friendlyZeroStateTitleBeforeName",
                      parts[0].substr(0, offsets[0]));
    source->AddString("friendlyZeroStateTitleAfterName",
                      parts[0].substr(offsets[0] + gaia_name.length()));
  } else {
    // Fallback to default behavior if name replacement fails.
    source->AddString("friendlyZeroStateGaiaName", "");
    source->AddString("friendlyZeroStateTitleBeforeName", "");
    source->AddString("friendlyZeroStateTitleAfterName", "");
  }
  source->AddString("friendlyZeroStateSubtitle",
                    parts.size() > 1 ? parts[1] : u"");
}

ContextualTasksUI::ContextualTasksUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/true,
                              /*enable_chrome_histograms=*/true),
      auto_suggestion_manager_(
          std::make_unique<
              contextual_tasks::ContextualTasksAutoSuggestionManager>()),
      ui_service_(contextual_tasks::ContextualTasksUiServiceFactory::
                      GetForBrowserContext(
                          web_ui->GetWebContents()->GetBrowserContext())),
      contextual_tasks_service_(
          contextual_tasks::ContextualTasksServiceFactory::GetForProfile(
              Profile::FromBrowserContext(
                  web_ui->GetWebContents()->GetBrowserContext()))) {
  Profile* profile = Profile::FromWebUI(web_ui);

  // Add a handler to provide plural strings.
  auto plural_string_handler = std::make_unique<PluralStringHandler>();
  plural_string_handler->AddLocalizedString("sharingTabs",
                                            IDS_COMPOSE_SHARING_TABS);
  web_ui->AddMessageHandler(std::move(plural_string_handler));

#if !BUILDFLAG(IS_ANDROID)
  // This hints the IPH system to use the webui help bubble factory. It is
  // necessary to avoid floating a Views bubble when side paneled (cobrowse)
  // because the webui modal dialog can only work well with a help bubble
  // when it is part of the dialog's DOM in the browser "top layer".
  ForceWebUIHelpBubbles::CreateForWebContents(web_ui->GetWebContents());
  if (auto* forced =
          ForceWebUIHelpBubbles::FromWebContents(web_ui->GetWebContents())) {
    forced->SetForceWebUIForAnchors({kSmartTabSharingMenuItemElementId});
  }
#endif

  // In MPArch, a single webcontents is used to host multiple frame trees rather
  // than having a separate webcontents for each. In that case there's no need
  // to wait for a webcontents to be created as they all live in the same one
  // that is hosting the webui. Attach the nav observer to this contents
  // directly.
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    nav_observer_ = std::make_unique<FrameNavObserver>(
        web_ui->GetWebContents(), ui_service_, contextual_tasks_service_, this);
  } else {
    inner_web_contents_creation_observer_ =
        std::make_unique<InnerFrameCreationObvserver>(
            web_ui->GetWebContents(),
            base::BindRepeating(&ContextualTasksUI::OnInnerWebContentsCreated,
                                weak_ptr_factory_.GetWeakPtr()),
            base::BindRepeating(&ContextualTasksUI::ResetEmbeddedPage,
                                weak_ptr_factory_.GetWeakPtr()));
  }

#if BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX)
  content::URLDataSource::Add(profile,
                              std::make_unique<SanitizedImageSource>(profile));
  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));
#endif

#if !BUILDFLAG(IS_ANDROID)
  host_zoom_map_subscription_ =
      content::HostZoomMap::GetDefaultForBrowserContext(profile)
          ->AddZoomLevelChangedCallback(base::BindRepeating(
              &ContextualTasksUI::OnZoomLevelChanged, base::Unretained(this)));
#endif
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIContextualTasksHost);
  webui::SetupWebUIDataSource(source, kContextualTasksResources,
                              IDR_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_HTML);

  AddInitialTaskStateToDataSource(source,
                                  web_ui->GetWebContents()->GetVisibleURL());

  std::string task_id_str;
  if (net::GetValueForKeyInQuery(web_ui->GetWebContents()->GetVisibleURL(),
                                 contextual_tasks::kTaskQueryParam,
                                 &task_id_str)) {
    base::Uuid task_id = base::Uuid::ParseLowercase(task_id_str);
    if (task_id.is_valid()) {
      task_id_ = task_id;
    }
  }

  // TODO(447633840): This is a placeholder URL until the real page is ready.
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' https://*.google.com;");

#if BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX)
  // Add required resources for the searchbox.
  bool session_allows_drag_and_drop = false;
  if (auto* session_handle = GetOrCreateContextualSessionHandle()) {
    session_allows_drag_and_drop =
        session_handle->CheckSearchContentSharingSettings(profile->GetPrefs());
  }

  source->AddLocalizedStrings(SearchboxHandler::GetWebUIDataSourceDict(
      profile, {.enable_voice_search = true,
                .session_allows_drag_and_drop = session_allows_drag_and_drop}));
#endif  // BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX)

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  auto bindings = web_ui->GetBindings();
  bindings.Put(content::BindingsPolicyValue::kSlimWebView);
  web_ui->SetBindings(bindings);
  source->AddResourcePaths(kGuestViewSharedResources);
#endif  // !BUILDFLAG(ENABLE_EXTENSIONS_CORE)

  // Add strings.js
  source->UseStringsJs();

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"closeTooltip", IDS_CONTEXTUAL_TASKS_SIDE_PANEL_CLOSE_TOOL_TIP},
      {"contextTooltip", IDS_CONTEXTUAL_TASKS_SIDE_PANEL_CONTEXT_TOOL_TIP},
      {"continueThread", IDS_CONTEXTUAL_TASKS_CONTINUE_THREAD_MESSAGE},
      {"feedback", IDS_LENS_SEND_FEEDBACK},
      {"help", IDS_CONTEXTUAL_TASKS_MENU_HELP},
      {"moreOptionsTooltip",
       IDS_CONTEXTUAL_TASKS_SIDE_PANEL_MORE_OPTIONS_TOOL_TIP},
      {"myActivity", IDS_CONTEXTUAL_TASKS_MENU_MY_ACTIVITY},
      {"newThreadTooltip", IDS_CONTEXTUAL_TASKS_SIDE_PANEL_NEW_THREAD_TOOL_TIP},
      {"openInNewTab", IDS_CONTEXTUAL_TASKS_MENU_OPEN_IN_NEW_TAB},
      {"pinTooltip", IDS_SIDE_PANEL_HEADER_PIN_BUTTON_TOOLTIP},
      {"reopenTab", IDS_CONTEXTUAL_TASKS_REOPEN_TABS_BUTTON_TEXT},
      {"sourcesMenuTitle", IDS_CONTEXTUAL_TASKS_SOURCES_MENU_TITLE},
      {"threadHistoryTooltip",
       IDS_CONTEXTUAL_TASKS_SIDE_PANEL_HISTORY_TOOL_TIP},
      {"title", IDS_CONTEXTUAL_TASKS_AI_MODE_TITLE},
      {"unpinTooltip", IDS_SIDE_PANEL_HEADER_UNPIN_BUTTON_TOOLTIP},
      /* composeDeepSearchPlaceholder and
       * composeCreateImagePlaceholder are defined by searchbox_handler.cc.
       */
      {"onboardingTitle", IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_TITLE},
      {"onboardingBody", IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_DESCRIPTION},
      {"onboardingLink", IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_LEARN_MORE},
      {"onboardingAcceptButton",
       IDS_CONTEXTUAL_TASKS_FIRST_RUN_EXPERIENCE_ACCEPT_BUTTON},
      {"oauthErrorDialogTitle", IDS_CONTEXTUAL_TASKS_OAUTH_ERROR_DIALOG_TITLE},
      {"oauthErrorDialogBody", IDS_CONTEXTUAL_TASKS_OAUTH_ERROR_DIALOG_BODY},
      {"oauthErrorDialogReloadButton",
       IDS_CONTEXTUAL_TASKS_OAUTH_ERROR_DIALOG_RELOAD_BUTTON},
      {"stsTryItLink", IDS_STS_IPH_TRY_IT_LINK},
      {"stsTryItBodyEnd", IDS_STS_IPH_TRY_IT_BODY_END},
      {"stsTryItTurnOn", IDS_STS_IPH_TRY_IT_TURN_ON},
      {"stsTryItNotNow", IDS_STS_IPH_TRY_IT_NOT_NOW},
      {"stsDefaultOnLink", IDS_STS_IPH_DEFAULT_ON_LINK},
      {"stsDefaultOnBodyEnd", IDS_STS_IPH_DEFAULT_ON_BODY_END},
      {"stsDefaultOnTurnOn", IDS_STS_IPH_DEFAULT_ON_TURN_ON},
      {"stsDefaultOnNotNow", IDS_STS_IPH_DEFAULT_ON_NOT_NOW},
#if !BUILDFLAG(IS_ANDROID)
      {"composeboxHintTextLensOverlay",
       IDS_LENS_COMPOSEBOX_HINT_TEXT_SELECT_PAGE},
#endif
  };
  source->AddLocalizedStrings(kLocalizedStrings);

  int stsDefaultOnHeaderId = IDS_STS_IPH_DEFAULT_ON_HEADER;
  int stsDefaultOnBodyId = IDS_STS_IPH_DEFAULT_ON_BODY;
  switch (contextual_tasks::kSmartTabSharingIphDefaultOnOption.Get()) {
    case contextual_tasks::SmartTabSharingIphDefaultOnOption::kIphDefaultOnV1:
      stsDefaultOnHeaderId = IDS_STS_IPH_DEFAULT_ON_HEADER;
      stsDefaultOnBodyId = IDS_STS_IPH_DEFAULT_ON_BODY;
      break;
    case contextual_tasks::SmartTabSharingIphDefaultOnOption::kIphDefaultOnV2:
      stsDefaultOnHeaderId = IDS_STS_IPH_DEFAULT_ON_HEADER_V2;
      stsDefaultOnBodyId = IDS_STS_IPH_DEFAULT_ON_BODY_V2;
      break;
  }
  source->AddLocalizedString("stsDefaultOnHeader", stsDefaultOnHeaderId);
  source->AddLocalizedString("stsDefaultOnBody", stsDefaultOnBodyId);

  int stsTryItHeaderId = IDS_STS_IPH_TRY_IT_HEADER;
  int stsTryItBodyId = IDS_STS_IPH_TRY_IT_BODY;
  switch (contextual_tasks::kSmartTabSharingIphTryItPromoOption.Get()) {
    case contextual_tasks::SmartTabSharingIphTryItPromoOption::kIphTryItPromoV1:
      stsTryItHeaderId = IDS_STS_IPH_TRY_IT_HEADER;
      stsTryItBodyId = IDS_STS_IPH_TRY_IT_BODY;
      break;
    case contextual_tasks::SmartTabSharingIphTryItPromoOption::kIphTryItPromoV2:
      stsTryItHeaderId = IDS_STS_IPH_TRY_IT_HEADER_V2;
      stsTryItBodyId = IDS_STS_IPH_TRY_IT_BODY_V2;
      break;
  }
  source->AddLocalizedString("stsTryItHeader", stsTryItHeaderId);
  source->AddLocalizedString("stsTryItBody", stsTryItBodyId);

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
  source->AddBoolean("lensSendRawFileMediaTypesEnabled",
                     lens::features::IsLensSendRawFileMediaTypesEnabled());

  source->AddString("nlmUrlParam",
                    contextual_tasks::GetContextualTasksNlmUrlParam());
  source->AddBoolean("enableCustomNlmUi",
                     contextual_tasks::IsCustomNlmUiEnabled());

  source->AddInteger(
      "composeboxFileMaxSize",
      contextual_tasks::kContextualTasksNextboxMaxFileSize.Get());
  // Enable typed suggest.
  source->AddBoolean("composeboxShowTypedSuggest", false);
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
  source->AddBoolean(
      "enablePinButton",
      contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled());
  source->AddBoolean(
      "isSidePanelPinned",
      contextual_tasks::IsContextualTasksPinButtonInToolbarEnabled() &&
          profile->GetPrefs()->GetBoolean(prefs::kPinContextualTaskButton));
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
      profile->GetPrefs()->GetInteger(
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
  source->AddBoolean("composeboxSmartComposeEnabled",
                     contextual_tasks::GetEnableContextualTasksSmartCompose());
  source->AddBoolean("enableNativeZeroStateSuggestions",
                     contextual_tasks::GetEnableNativeZeroStateSuggestions());
  // Contextual tasks needs finer control over when to query zps based on its
  // current state. Most other composebox's blindly query autocomplete as soon
  // as it is rendered.
  source->AddBoolean("queryZpsOnLoad", false);

  AddContextMenuItemEligibilityLoadTimeData(source, profile);
  source->AddBoolean("composeboxShowLensSearchChip", false);
  source->AddBoolean("composeboxShowContextMenuTabPreviews", false);
  source->AddBoolean("composeboxContextMenuEnableMultiTabSelection", true);
  source->AddBoolean("enableGhostLoader",
                     contextual_tasks::GetIsGhostLoaderEnabled());
  source->AddBoolean(
      "forceBasicModeIfOpeningThreadHistory",
      contextual_tasks::ShouldForceBasicModeIfOpeningThreadHistory());
  source->AddBoolean("enableBasicMode",
                     contextual_tasks::GetIsBasicModeEnabled());
  source->AddBoolean("enableBasicModeZOrder",
                     contextual_tasks::ShouldEnableBasicModeZOrder());
  source->AddBoolean(
      "enableLockAndUnlockInputCapability",
      contextual_tasks::ShouldEnableLockAndUnlockInputCapability());
  source->AddBoolean("enableFileHint", contextual_tasks::GetEnableFileHint());
  source->AddBoolean("supportsLensButtonInComposebox", !BUILDFLAG(IS_ANDROID));
  source->AddBoolean("isSystemVoiceSearchEnabled", BUILDFLAG(IS_ANDROID));
  source->AddBoolean("enableComposeboxJumpFix",
                     contextual_tasks::GetEnableComposeboxJumpFix());
  source->AddBoolean("roundedClipPathEnabled",
                     contextual_tasks::IsRoundedClipPathEnabled());
  source->AddBoolean("hideMenuOnAiPageEnabled",
                     base::FeatureList::IsEnabled(
                         contextual_tasks::kContextualTasksHideMenuOnAiPage));
  source->AddBoolean(
      "contextManagementInComposeboxEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox));
  source->AddBoolean(
      "tabFaviconChipsToCoinsEnabled",
      base::FeatureList::IsEnabled(omnibox::kTabFaviconChipsToCoins));

  source->AddString(
      "composeboxSource",
      contextual_search::ContextualSearchMetricsRecorder::
          ContextualSearchSourceToString(
              contextual_search::ContextualSearchSource::kContextualTasks));
#if !BUILDFLAG(IS_ANDROID)
  GURL url = web_ui->GetWebContents()->GetVisibleURL();
  bool is_dark_mode =
      ThemeServiceFactory::GetForProfile(profile)->BrowserUsesDarkColors();
  is_dark_mode =
      contextual_tasks::GetDarkModeFromUrl(url).value_or(is_dark_mode);
  source->AddBoolean("darkMode", is_dark_mode);
  source->AddLocalizedString(
      "protectedErrorPageTopLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_FIRST_LINE);
  source->AddLocalizedString(
      "protectedErrorPageBottomLine",
      IDS_SIDE_PANEL_LENS_OVERLAY_PROTECTED_PAGE_ERROR_SECOND_LINE);
#else
  // TODO(crbug.com/483442073): Replace the values with Android resources.
  bool is_dark_mode = web_ui->GetWebContents()->GetColorMode() ==
                      ui::ColorProviderKey::ColorMode::kDark;
  source->AddBoolean("darkMode", is_dark_mode);
  source->AddString("protectedErrorPageTopLine", "string");
  source->AddString("protectedErrorPageBottomLine", "string");
#endif

  source->AddString("userAgentSuffix",
                    contextual_tasks::GetContextualTasksUserAgentSuffix());
  // Preload the serialized handshake message so it doesn't have to be fetched
  // at runtime.
  source->AddString("handshakeMessage", GetEncodedHandshakeMessage());

  source->AddBoolean("isSmallDeviceFormFactor",
                     ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE);

  // Force a host for any URL opened in the embedded page. If empty, no change
  // is made to the URL.
  source->AddString("forcedEmbeddedPageHost",
                    contextual_tasks::GetForcedEmbeddedPageHost());
  source->AddString(
      "contextualTasksSignInDomains",
      base::JoinString(contextual_tasks::GetContextualTasksSignInDomains(),
                       ","));

  // Determine and cache contextual tasks eligibility on initialization. This
  // prevents the expand button from dynamically appearing or changing state
  // mid-session, avoiding a jarring user experience.
  is_contextual_tasks_eligible_on_init_ =
      ui_service_ && ui_service_->GetEligibilityManager() &&
      ui_service_->GetEligibilityManager()->IsEligible();

  // Expand button experiment state.
  source->AddBoolean(
      "expandButtonEnabled",
      is_contextual_tasks_eligible_on_init_ &&
          contextual_tasks::GetExpandButtonOption() ==
              contextual_tasks::ExpandButtonOption::kSidePanelExpandButton);

  source->AddBoolean("caretAnimationEnabled",
                     base::FeatureList::IsEnabled(
                         contextual_tasks::kContextualTasksAnimatedCaret));

  source->AddBoolean(
      "energyEffectEnabled",
      base::FeatureList::IsEnabled(contextual_tasks::kEnergyEffectInNextbox));

  // Set up chrome://contextual-tasks/internals debug UI.
  source->AddResourcePath(
      "internals",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);
  source->AddResourcePath(
      "internals/",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);

  source->AddBoolean(
      "useStratusDarkModeColors",
      contextual_tasks::ShouldUseStratusDarkModeColors());
  source->AddString(
      "useStratusDarkModeColorsAttr",
      contextual_tasks::ShouldUseStratusDarkModeColors() ? "true" : "false");

  source->AddBoolean("smartTabSharingEnabled",
                     contextual_tasks::ContextualTasksContextService::
                         GetIsSmartTabSharingEnabled(profile));

  source->AddBoolean(
      "enableContextManagementInComposebox",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox));

  AddZeroStateStrings(source, profile);
  contextual_tasks_service_observation_.Observe(contextual_tasks_service_);

#if !BUILDFLAG(IS_ANDROID)
  ui::TrackedElementHandlerDocumentSingleton::Register(
      this,
      std::vector<ui::ElementIdentifier>{kSmartTabSharingMenuItemElementId});
#endif
}

ContextualTasksUI::~ContextualTasksUI() {
  if (ui_service_) {
    ui_service_->OnWebUIDestroyed(GetBrowser(), task_id_);
  }
}

void ContextualTasksUI::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> page_handler) {
  // Reset the page and page handler before binding in case they already exists
  // (like on a reload). Not resetting them can cause unintended behavior.
  page_.reset();
  page_handler_.reset();

  page_.Bind(std::move(page));
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      std::move(page_handler), this, ui_service_, contextual_tasks_service_,
      GetPanelController());

#if !BUILDFLAG(IS_ANDROID)
  // Determine if the Lens overlay is showing when the page is created.
  if (auto* browser = GetBrowser()) {
    if (auto* controller = LensSearchController::FromTabWebContents(
            browser->GetTabStripModel()->GetActiveWebContents())) {
      OnLensOverlayStateChanged(controller->IsShowingUI(),
                                controller->invocation_source());
    }
  }
#endif
  // If a task is already active when the page handler is created (e.g., on
  // page reload), restore the WebUI state by pushing the task details. If the
  // inner frame URL is not yet available, fallback to the task's creation URL.
  if (task_id_ && web_ui() && web_ui()->GetWebContents()) {
    GURL url = GetInnerFrameUrl();
    if (url.is_empty() && ui_service_) {
      url =
          ui_service_->GetCreationUrlForTask(task_id_.value()).value_or(GURL());
    }
    PushTaskDetailsToPage(task_id_, url,
                          /*replace_navigation_entry=*/true);
  }
  OnInitComplete();
}

void ContextualTasksUI::OnRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info) {}

void ContextualTasksUI::OnZeroStateChange(bool is_zero_state) {
  if (page_) {
    page_->OnZeroStateChange(is_zero_state);
  }
}

void ContextualTasksUI::OnTaskUpdated(
    const contextual_tasks::ContextualTask& task,
    contextual_tasks::ContextualTasksService::TriggerSource source) {
  if (task_id_ && task_id_.value() == task.GetTaskId()) {
    // Update the auto suggested tab chip if needed.
    OnActiveTabContextStatusChanged();
  }
}

const std::optional<base::Uuid>& ContextualTasksUI::GetTaskId() {
  return task_id_;
}

void ContextualTasksUI::SetTaskId(std::optional<base::Uuid> id) {
  // Only clear restored tabs if the task has changed or no id exists.
  if (base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox) &&
      ((id.has_value() && task_id_.has_value() &&
        id.value() != task_id_.value()) ||
       !id.has_value())) {
    OnRestoredTabsFetched({});
  }
  task_id_ = id;
  // Initialize input state once task id is available.
  if (composebox_handler_) {
    composebox_handler_->InitializeInputStateModel();
  }
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

void ContextualTasksUI::UpdateStateFromUrl(const GURL& url) {
  if (composebox_handler_) {
    composebox_handler_->UpdateStateFromUrl(url);
  }
}

void ContextualTasksUI::SetInNlm(bool in_nlm) {
  if (page_) {
    page_->SetInNlm(in_nlm);
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
#if !BUILDFLAG(IS_ANDROID)
      if (auto* controller = LensSearchController::FromTabWebContents(
              browser->GetTabStripModel()->GetActiveWebContents())) {
        controller->CloseLensAsync(
            lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted);
      }
#endif
    }
  }
  was_ai_page_ = is_ai_page;

  auto* panel_controller = GetPanelController();
  if (panel_controller) {
    panel_controller->NotifyExpandToFullTabStateChanged();
  }
}

const GURL& ContextualTasksUI::GetInnerFrameUrl() const {
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch)) {
    return nav_observer_ ? nav_observer_->last_committed_url()
                         : GURL::EmptyGURL();
  }

  if (!nav_observer_ || !nav_observer_->web_contents()) {
    return GURL::EmptyGURL();
  }

  return nav_observer_->web_contents()->GetLastCommittedURL();
}

content::WebContents* ContextualTasksUI::GetInnerWebContents() const {
  return embedded_web_contents_.get();
}

bool ContextualTasksUI::IsInitComplete() {
  return page_handler_ != nullptr;
}

void ContextualTasksUI::OnInitComplete() {
  if (task_id_ && ui_service_) {
    ui_service_->OnWebUIReady(GetBrowser(), *task_id_,
                              web_ui()->GetWebContents());
  }

  for (auto& observer : observers_) {
    observer.OnInitComplete();
  }
}

void ContextualTasksUI::AddObserver(
    contextual_tasks::ContextualTasksUIInterface::Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksUI::RemoveObserver(
    contextual_tasks::ContextualTasksUIInterface::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ContextualTasksUI::IsShownInTab() {
  return tabs::TabInterface::MaybeGetFromContents(web_ui()->GetWebContents());
}

BrowserWindowInterface* ContextualTasksUI::GetBrowser() {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  BrowserWindowInterface* window = FromWebContents(web_contents);
  if (window) {
    return window;
  }
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  if (tab) {
    return tab->GetBrowserWindowInterface();
  }
  return nullptr;
}

Profile* ContextualTasksUI::GetProfile() {
  return Profile::FromWebUI(web_ui());
}

contextual_tasks::ContextualTasksAutoSuggestionManager*
ContextualTasksUI::GetAutoSuggestionManager() {
  return auto_suggestion_manager_.get();
}

content::WebContents* ContextualTasksUI::GetWebUIWebContents() {
  return web_ui()->GetWebContents();
}

void ContextualTasksUI::CloseSidePanel() {
  auto* controller = GetPanelController();
  if (!controller) {
    return;
  }

  controller->Close();
}

void ContextualTasksUI::BindInterface(
    mojo::PendingReceiver<contextual_tasks::mojom::PageHandlerFactory>
        pending_receiver) {
  contextual_tasks_page_handler_factory_receiver_.reset();
  contextual_tasks_page_handler_factory_receiver_.Bind(
      std::move(pending_receiver));
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualTasksUI::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  help_bubble_factory_receiver_.reset();
  help_bubble_factory_receiver_.Bind(std::move(pending_receiver));
}

void ContextualTasksUI::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
          web_ui()->GetRenderFrameHost()));
}

#endif

bool ContextualTasksUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  // Disable for OTR profiles.
  return base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
         !browser_context->IsOffTheRecord();
}

bool ContextualTasksUIConfig::ShouldCrashOnJavascriptErrorInDevelopmentBuild()
    const {
  return true;
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
  auto handler = std::make_unique<ContextualTasksComposeboxHandler>(
      this, Profile::FromWebUI(web_ui()), web_ui()->GetWebContents(),
      std::move(pending_page_handler), std::move(pending_page),
      std::move(pending_searchbox_handler), std::move(pending_searchbox_page),
      base::BindRepeating(
          &ContextualTasksUI::GetOrCreateContextualSessionHandle,
          base::Unretained(this)),
      base::BindRepeating(&ContextualTasksUI::ClearContextualSessionHandle,
                          base::Unretained(this)),
      base::BindRepeating(&ContextualTasksUI::TakeInputStateModel,
                          base::Unretained(this)));
  owned_composebox_handler_ = std::move(handler);
  SetComposeboxHandler(owned_composebox_handler_.get());

  // Sync the initial auto-suggestion state.
  composebox_handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_->GetCurrentSuggestion());
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
          contextual_tasks::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kContextualTasks,
          lens::LensOverlayInvocationSource::kContextualTasksComposebox);
      // TODO(crbug.com/469875164): Determine what to do with the return value
      // of this call, or move this call to a different location.
      session_handle->CheckSearchContentSharingSettings(
          Profile::FromWebUI(web_ui())->GetPrefs());
      helper->SetTaskSession(std::nullopt, std::move(session_handle),
                             /*input_state_model=*/nullptr);
      return helper->session_handle();
    }
  }

  // If no valid session exists, maintains context continuity by trying to find
  // one from affiliated tabs or side panel WebContents.
  contextual_tasks::ContextualTasksPanelController* controller =
      GetPanelController();
  if (!controller || !task_id_.has_value()) {
    return nullptr;
  }

  auto* browser_window_interface =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  UpdateContextualSearchWebContentsHelperForTask(
      contextual_search_service,
      /*browser_window=*/browser_window_interface, contextual_tasks_service_,
      controller, web_contents, task_id_.value());
  return helper->session_handle();
}

GURL ContextualTasksUI::GetWebUiUrl() {
  return web_ui()->GetWebContents()->GetLastCommittedURL();
}

// Empty implementation, does not need to be cleared in contextual tasks. Only
// needs to be cleared when transferring ownership to a new web contents / UI
// controller which never happens for contextual tasks.
void ContextualTasksUI::ClearContextualSessionHandle() {}

std::unique_ptr<contextual_search::InputStateModel>
ContextualTasksUI::TakeInputStateModel() {
  if (!task_id_.has_value()) {
    return nullptr;
  }

  content::WebContents* web_contents = web_ui()->GetWebContents();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  return helper->TakeInputStateModelForTask(task_id_.value());
}

std::vector<int32_t> ContextualTasksUI::GetRestoredTabIds() {
  if (!task_id_.has_value()) {
    return {};
  }

  content::WebContents* web_contents = web_ui()->GetWebContents();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  return helper->GetSelectedTabIdsForTask(task_id_.value());
}

void ContextualTasksUI::SetComposeboxHandler(
    contextual_tasks::ContextualTasksComposeboxHandlerInterface* handler) {
  composebox_handler_ = handler;
}

void ContextualTasksUI::SetComposeboxHandlerForTesting(  // IN-TEST
    std::unique_ptr<contextual_tasks::ContextualTasksComposeboxHandlerInterface>
        handler) {
  owned_composebox_handler_ = std::move(handler);
  SetComposeboxHandler(owned_composebox_handler_.get());
}

void ContextualTasksUI::MoveTaskUiToNewTab() {
  auto* browser = GetBrowser();
  if (!task_id_.has_value()) {
    return;
  }

  ui_service_->MoveTaskUiToNewTab(task_id_.value(), browser,
                                  GetInnerFrameUrl());
}

void ContextualTasksUI::PostMessageToWebview(
    const lens::ClientToAimMessage& message) {
  CHECK(page_handler_);
  page_handler_->PostMessageToWebview(message);
}

void ContextualTasksUI::ShowOauthErrorDialog() {
  if (page_) {
    page_->ShowOauthErrorDialog();
  }
}

void ContextualTasksUI::OnInnerWebContentsCreated(
    content::WebContents* inner_contents) {
  // This is assumed to only be called once per WebUI lifetime. Can be called
  // multiple times if the WebUI is reloaded, but that would have reset
  // `embedded_web_contents_`.
  if (embedded_web_contents_) {
    return;
  }

  nav_observer_ = std::make_unique<FrameNavObserver>(
      inner_contents, ui_service_, contextual_tasks_service_, this);
  embedded_web_contents_ = inner_contents->GetWeakPtr();

  // Trigger the cookie sync now that the embedded page is created. This is a
  // fire and forget call, assuming the cookie sync will succeed eventually, and
  // relying on OAuth tokens until then.
  ui_service_->EnsureCookiesSynced();
}

void ContextualTasksUI::OnContextRetrievedForActiveTab(
    base::WeakPtr<BrowserWindowInterface> browser,
    int32_t tab_id,
    const GURL& last_committed_url,
    std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
  // Do nothing is the webUI is no longer in side panel.
  if (IsShownInTab()) {
    return;
  }

  if (!browser) {
    return;
  }
  TabListInterface* tab_list = TabListInterface::From(browser.get());
  if (!tab_list) {
    return;
  }
  tabs::TabInterface* tab = tab_list->GetActiveTab();

  // If active tab or tab URL changed since the GetContextForTask() call, do
  // nothing.
  if (!tab || tab->GetHandle().raw_value() != tab_id ||
      tab->GetContents()->GetLastCommittedURL() != last_committed_url) {
    return;
  }

  // If last_committed_url is already in the context, clear the suggested tab
  // context.
  std::unique_ptr<url_deduplication::URLDeduplicationHelper>
      url_duplication_helper =
          contextual_tasks::CreateURLDeduplicationHelperForContextualTask();
  bool is_tab_already_in_context =
      context &&
      context->ContainsURL(last_committed_url, url_duplication_helper.get());

  std::unique_ptr<contextual_tasks::SuggestedTabInfo> suggestion;
  if (!is_tab_already_in_context) {
    content::WebContents* web_contents = tab->GetContents();
    suggestion = std::make_unique<contextual_tasks::SuggestedTabInfo>();
    suggestion->tab_id = tab->GetHandle().raw_value();
    suggestion->title = web_contents->GetTitle();
    suggestion->url = web_contents->GetLastCommittedURL();
    suggestion->last_active =
        std::max(web_contents->GetLastActiveTimeTicks(),
                 web_contents->GetLastInteractionTimeTicks());
  }

  auto_suggestion_manager_->SetCurrentSuggestion(std::move(suggestion));
  if (composebox_handler_) {
    composebox_handler_->UpdateSuggestedTabContext(
        auto_suggestion_manager_->GetCurrentSuggestion());
  }
}

void ContextualTasksUI::AddInitialTaskStateToDataSource(
    content::WebUIDataSource* source,
    const GURL& url) {
  // Set initial state based on task state to avoid UI flickers.
  std::string task_id_str;
  base::Uuid task_id;
  if (net::GetValueForKeyInQuery(url, contextual_tasks::kTaskQueryParam,
                                 &task_id_str)) {
    task_id = base::Uuid::ParseLowercase(task_id_str);
  }

  std::string host_value;
  if (net::GetValueForKeyInQuery(url, contextual_tasks::kChromeHostParam,
                                 &host_value)) {
    if (contextual_tasks::ContextualTasksUiService::IsTrustedHost(host_value)) {
      source->AddString(contextual_tasks::kChromeHostParam, host_value);
    }
  }

  std::optional<GURL> task_creation_url =
      ui_service_ ? ui_service_->GetCreationUrlForTask(task_id) : std::nullopt;
  bool show_ghost_loader = task_creation_url && task_creation_url->is_empty();
  source->AddBoolean("isGhostLoaderVisible", show_ghost_loader);
  source->AddBoolean("isAiPage",
                     ui_service_ && task_creation_url &&
                         ui_service_->IsAiUrl(task_creation_url.value()));
}

void ContextualTasksUI::OnSidePanelStateChanged() {
  page_->OnSidePanelStateChanged();

  lens::ClientToAimMessage message;
  auto* display_mode_msg = message.mutable_set_cobrowsing_display_mode();
  if (IsShownInTab()) {
    display_mode_msg->mutable_params()->set_display_mode(
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
    display_mode_msg->mutable_params()->set_display_mode(
        lens::CobrowsingDisplayModeParams::COBROWSING_SIDEPANEL);
  }

  PostMessageToWebview(message);

#if !BUILDFLAG(IS_ANDROID)
  UpdateZoom();
#endif
}

void ContextualTasksUI::OnLensOverlayStateChanged(
    bool is_showing,
    std::optional<lens::LensOverlayInvocationSource> invocation_source) {
  is_lens_overlay_showing_ = is_showing;
  if (page_) {
    bool maybe_show_overlay_hint_text =
        is_showing && invocation_source.has_value() &&
        invocation_source.value() ==
            lens::LensOverlayInvocationSource::kContextualTasksComposebox;
    page_->OnLensOverlayStateChanged(is_showing, maybe_show_overlay_hint_text);
  }
}

bool ContextualTasksUI::IsLensOverlayShowing() const {
  return is_lens_overlay_showing_;
}

void ContextualTasksUI::StartPlatformVoiceRecognition() {
  ui_service_->StartPlatformVoiceRecognition(GetBrowser(),
                                             GetWebUIWebContents());
}

void ContextualTasksUI::OnVoiceTranscribed(const std::string& query) {
  auto& page = GetPageRemote();
  if (!page.is_bound()) {
    return;
  }
  auto input = contextual_tasks::mojom::InjectedInput::New();
  input->query_text = query;
  input->submit_after_injection = true;
  page->InjectInput(std::move(input));
}

bool ContextualTasksUI::CanUpdateSuggestedTabContext(
    tabs::TabInterface* tab,
    const GURL& last_committed_url) {
  if (!GetBrowser()) {
    return false;
  }

  if (!composebox_handler_) {
    return false;
  }

  if (!tab) {
    return false;
  }

  contextual_tasks::SiteExclusionDetail site_exclusion_detail;
  if (!contextual_tasks::IsValidUrlForSuggestedTab(
          last_committed_url, GetProfile(), site_exclusion_detail)) {
    return false;
  }

  if (!GetOrCreateContextualSessionHandle()) {
    return false;
  }

  return true;
}

void ContextualTasksUI::OnActiveTabContextStatusChanged() {
  if (contextual_tasks::GetIsProtectedPageErrorEnabled() && page_) {
    page_->HideErrorPage();
  }

  BrowserWindowInterface* browser = GetBrowser();
  TabListInterface* tab_list =
      browser ? TabListInterface::From(browser) : nullptr;
  tabs::TabInterface* tab = tab_list ? tab_list->GetActiveTab() : nullptr;
  GURL last_committed_url =
      tab ? tab->GetContents()->GetLastCommittedURL() : GURL::EmptyGURL();

  // Since `task_id_` can be set by external callers, capture it locally to
  // avoid crash caused by change between `has_value()` check here and `value()`
  // use below.
  std::optional<base::Uuid> task_id = GetTaskId();
  if (!CanUpdateSuggestedTabContext(tab, last_committed_url) ||
      !task_id.has_value()) {
    // Inform the handler that the current tab cannot be added as an autochip.
    auto_suggestion_manager_->SetCurrentSuggestion(nullptr);
    if (composebox_handler_) {
      composebox_handler_->UpdateSuggestedTabContext(nullptr);
    }
    return;
  }

  auto context_decoration_params =
      std::make_unique<contextual_tasks::ContextDecorationParams>();
  context_decoration_params->contextual_search_session_handle =
      GetOrCreateContextualSessionHandle()->AsWeakPtr();
  contextual_tasks_service_->GetContextForTask(
      task_id.value(),
      {contextual_tasks::ContextualTaskContextSource::kUploadedContextDecorator,
       contextual_tasks::ContextualTaskContextSource::
           kSubmittedContextDecorator},
      std::move(context_decoration_params),
      base::BindOnce(&ContextualTasksUI::OnContextRetrievedForActiveTab,
                     weak_ptr_factory_.GetWeakPtr(), browser->GetWeakPtr(),
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
    contextual_tasks::ShowAndRecordErrorPage(
        page_, contextual_search::ContextualSearchSource::kContextualTasks);
  }
}

void ContextualTasksUI::TransferNavigationToEmbeddedPage(
    content::OpenURLParams params) {
  OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
             "TransferNavigationToEmbeddedPage called for URL: "
          << params.url;
  bool is_allowed_url = ui_service_->IsValidSearchResultsPage(params.url) ||
                        ui_service_->IsAiUrl(params.url);
  if (!embedded_web_contents_ || !is_allowed_url) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: TransferNavigationToEmbeddedPage "
           "returning early because embedded_web_contents_="
        << !!embedded_web_contents_ << " is_allowed_url=" << is_allowed_url;
    return;
  }

  // TODO(465498890): Consider clearning source_site_instance in this case
  //                  since the navigation may be targeting a different storage
  //                  partition.
  params.frame_tree_node_id =
      embedded_web_contents_->GetPrimaryMainFrame()->GetFrameTreeNodeId();
  OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
             "TransferNavigationToEmbeddedPage opening URL in embedded page";
  embedded_web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
}

bool ContextualTasksUI::IsActiveTabContextSuggestionShowing() const {
  return auto_suggestion_manager_ &&
         auto_suggestion_manager_->GetCurrentSuggestion() != nullptr;
}

void ContextualTasksUI::PushTaskDetailsToPage(std::optional<base::Uuid> id,
                                              const GURL& url,
                                              bool replace_navigation_entry) {
  page_->SetTaskDetails(id.value_or(base::Uuid()), url,
                        replace_navigation_entry);
#if !BUILDFLAG(IS_ANDROID)
  tracked_zoom_host_ = url.host();
  UpdateZoom();
#endif
}

bool ContextualTasksUI::CanExpandToFullTab() const {
  // Employs the cached contextual tasks eligibility value calculated on
  // initialization. Mid-session updates are ignored to ensure the expand
  // affordance remains static and consistent.
  return was_ai_page_ && is_contextual_tasks_eligible_on_init_;
}

mojo::Remote<contextual_tasks::mojom::Page>&
ContextualTasksUI::GetPageRemote() {
  return page_;
}

contextual_tasks::ContextualTasksPanelController*
ContextualTasksUI::GetPanelController() {
  if (!web_ui()->GetWebContents()) {
    return nullptr;
  }

  auto* browser = webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return nullptr;
  }

  return contextual_tasks::ContextualTasksPanelController::From(browser);
}

ContextualTasksUI::FrameNavObserver::FrameNavObserver(
    content::WebContents* web_contents,
    contextual_tasks::ContextualTasksUiService* ui_service,
    contextual_tasks::ContextualTasksService* contextual_tasks_service,
    contextual_tasks::TaskInfoDelegate* task_info_delegate)
    : content::WebContentsObserver(web_contents),
      ui_service_(ui_service),
      contextual_tasks_service_(contextual_tasks_service),
      task_info_delegate_(CHECK_DEREF(task_info_delegate)) {}

void ContextualTasksUI::FrameNavObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
             "FrameNavObserver::DidFinishNavigation called";
  if (!ui_service_ || !contextual_tasks_service_) {
    return;
  }

  // Ignore uncommitted navigations.
  if (!navigation_handle->HasCommitted()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "FrameNavObserver::DidFinishNavigation returning early, not "
           "committed";
    return;
  }

  // With MPArch, the webview lives in the same WebContents as the webui page.
  // Make sure any navigations in this case are actually from the guest view.
  if (base::FeatureList::IsEnabled(features::kGuestViewMPArch) &&
      !navigation_handle->IsGuestViewMainFrame()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "FrameNavObserver::DidFinishNavigation returning early, not "
           "guest frame in MPArch";
    return;
  }

  // Ignore sub-frame navigations. Even with MPArch enabled, navigations
  // from the webview are still considered "main frame".
  if (!navigation_handle->IsInMainFrame()) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "FrameNavObserver::DidFinishNavigation returning early, not "
           "main frame";
    return;
  }

  auto current_title = task_info_delegate_->GetThreadTitle();

  // Notify the WebUI if the new page is an AI page so it can adjust the UI
  // accordingly.
  const GURL& url = navigation_handle->GetURL();
  OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
             "FrameNavObserver::DidFinishNavigation URL: "
          << url;
  bool is_ai_page = ui_service_->IsAiUrl(url);
  task_info_delegate_->SetIsAiPage(is_ai_page);

#if BUILDFLAG(IS_ANDROID)
  // On Android, the toolbar needs to be explicitly notified to refresh its
  // display URL when the inner frame navigates, as the main tab's URL
  // (chrome://contextual-tasks) hasn't changed.
  CHECK(web_contents());
  web_contents()->NotifyNavigationStateChanged(content::INVALIDATE_TYPE_URL);
#endif

  bool in_nlm = false;
  std::string value;
  if (net::GetValueForKeyInQuery(
          url, contextual_tasks::GetContextualTasksNlmUrlParam(), &value)) {
    in_nlm = true;
  }
  task_info_delegate_->SetInNlm(in_nlm);

  task_info_delegate_->UpdateStateFromUrl(url);

  OMNIBOX_LOG("embedded_page_nav") << navigation_handle->GetURL().spec();

  // Set whether this navigation is to a zero state so the UI can adjust
  // accordingly.
  const bool is_zero_state = ContextualTasksUI::IsZeroState(url, ui_service_);

  // Record the HTTP response code of the inner frame contents if response
  // headers are available.
  if (auto* response_headers = navigation_handle->GetResponseHeaders()) {
    contextual_tasks::RecordInnerFrameContentsHttpResponseCode(
        response_headers->response_code(), is_zero_state);
  }

  // Check if the zero state status has changed since the last navigation.
  const bool has_zero_state_changed =
      is_zero_state !=
      ContextualTasksUI::IsZeroState(last_committed_url_, ui_service_);

  if (!navigation_handle->IsSameDocument() || has_zero_state_changed) {
    task_info_delegate_->OnZeroStateChange(is_zero_state);
  }

  // Adjust the preference for dark mode to respect the CS param. This prevents
  // a UI flicker that would happen if the CS param mismatches the browser
  // settings.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->IsSameDocument()) {
    UpdateDarkModePreferenceFromUrl(web_contents(), url);
  }
  bool is_url_changed = false;
  bool last_committed_url_was_empty = last_committed_url_.is_empty();
  if (!ContextualTasksUI::AreUrlsEqual(
          url, last_committed_url_)) {
    last_committed_url_ = url;
    is_url_changed = true;
  }

  if (!is_url_changed) {
    OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
               "FrameNavObserver::DidFinishNavigation returning early, URL "
               "unchanged";
    return;
  }

  if (!is_ai_page) {
    OMNIBOX_LOG("nav_trace")
        << "ContextualTasks navigation trace: "
           "FrameNavObserver::DidFinishNavigation returning early, not AI page";
    return;
  }

  // Capture the old task ID associated with this WebUI.
  std::optional<base::Uuid> old_task_id = task_info_delegate_->GetTaskId();
  if (is_zero_state &&
      (!base::FeatureList::IsEnabled(
           contextual_tasks::kEnableNotifyZeroStateRenderedCapability) ||
       navigation_handle->IsSameDocument())) {
    OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
               "FrameNavObserver::DidFinishNavigation zero state logic";
    // Create a new task for zero state, since there's no thread to associate
    // this with yet.
    contextual_tasks::ContextualTask task =
        contextual_tasks_service_->CreateTask();
    base::Uuid new_task_id = task.GetTaskId();
    task_info_delegate_->SetTaskId(new_task_id);
    task_info_delegate_->SetThreadId(std::nullopt);
    // Replace state if last committed URL was empty (i.e. the page is
    // reloaded) or if zero state has not changed, otherwise push new state.
    task_info_delegate_->PushTaskDetailsToPage(
        new_task_id, url,
        /*replace_navigation_entry=*/last_committed_url_was_empty ||
            !has_zero_state_changed);
    task_info_delegate_->SetThreadTitle(std::nullopt);

    task_info_delegate_->PrepareForTaskChange();
    ui_service_->OnTaskChanged(task_info_delegate_->GetBrowser(),
                               task_info_delegate_->GetWebUIWebContents(),
                               old_task_id, new_task_id,
                               task_info_delegate_->IsShownInTab());
    task_info_delegate_->OnTaskChanged();
    return;
  }

  std::string query_value;
  net::GetValueForKeyInQuery(url, "q", &query_value);

  std::string url_thread_id;
  if (!net::GetValueForKeyInQuery(url, "mtid", &url_thread_id)) {
    OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
               "FrameNavObserver::DidFinishNavigation returning early, no "
               "mtid in URL";
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
        (pending_task_title_mismatch || is_new_conversation ||
         is_thread_switch) &&
        (!base::FeatureList::IsEnabled(
             omnibox::kContextManagementInComposebox) ||
         !task_info_delegate_->GetTaskId().has_value());

    if (should_create_new_task) {
      OMNIBOX_LOG("nav_trace") << "ContextualTasks navigation trace: "
                 "FrameNavObserver::DidFinishNavigation "
                 "should_create_new_task is true";
      task_changed = true;
      auto task = contextual_tasks_service_->CreateTaskFromUrl(url);
      task_info_delegate_->SetTaskId(task.GetTaskId());
    }
  }

  task_info_delegate_->SetThreadId(url_thread_id);
  auto new_task_id = task_info_delegate_->GetTaskId();
  // Replace state if old task id and new task id is the same, otherwise push
  // new state. This is to make sure navigation stack works when switching
  // between tasks.
  bool replace_navigation_entry = (old_task_id == new_task_id);
  task_info_delegate_->PushTaskDetailsToPage(new_task_id, url,
                                             replace_navigation_entry);

  // Update title after navigation to make sure the current navigation updates.
  if (!query_value.empty()) {
    task_info_delegate_->SetThreadTitle(query_value);
  }

  std::optional<std::string> mstk;
  std::string url_param_mstk;
  if (net::GetValueForKeyInQuery(url, "mstk", &url_param_mstk)) {
    mstk = url_param_mstk;
  }

  contextual_tasks_service_->UpdateThreadForTask(
      task_info_delegate_->GetTaskId().value(),
      contextual_tasks::ThreadType::kAiMode, url_thread_id, mstk,
      task_info_delegate_->GetThreadTitle());

  if (task_changed) {
    OMNIBOX_LOG("embedded_page_nav")
        << "Task changed: "
        << task_info_delegate_->GetTaskId().value().AsLowercaseString();
    task_info_delegate_->PrepareForTaskChange();
    ui_service_->OnTaskChanged(task_info_delegate_->GetBrowser(),
                               task_info_delegate_->GetWebUIWebContents(),
                               old_task_id, task_info_delegate_->GetTaskId(),
                               task_info_delegate_->IsShownInTab());
    task_info_delegate_->OnTaskChanged();
  }
}

bool ContextualTasksUI::IsZeroState(
    const GURL& url,
    contextual_tasks::ContextualTasksUiService* ui_service) {
  std::string query_value;
  std::string mstk_value;
  std::string smstk_value;
  std::string vsrid_value;
  std::string cinpts_value;
  net::GetValueForKeyInQuery(url, "q", &query_value);
  net::GetValueForKeyInQuery(url, "mstk", &mstk_value);
  net::GetValueForKeyInQuery(url, "smstk", &smstk_value);
  net::GetValueForKeyInQuery(url, "vsrid", &vsrid_value);
  net::GetValueForKeyInQuery(url, "cinpts", &cinpts_value);

  // If the URL is an AI URL and there's no query or (s)mstk, it's zero state.
  // If there is either a query or (s)mstk, assume it's not zero state. If there
  // is a vsrid/cinpts, assume it's not zero state since there will soon be an
  // mstk.
  // TODO(crbug.com/472336339): Find a more robust way to determine if the page
  // is zero state instead of query params.
  return ui_service->IsAiUrl(url) && query_value.empty() &&
         mstk_value.empty() && smstk_value.empty() && vsrid_value.empty() &&
         cinpts_value.empty();
}

ContextualTasksUI::InnerFrameCreationObvserver::InnerFrameCreationObvserver(
    content::WebContents* web_contents,
    base::RepeatingCallback<void(content::WebContents*)> callback,
    base::RepeatingClosure reset_callback)
    : content::WebContentsObserver(web_contents),
      callback_(std::move(callback)),
      reset_callback_(std::move(reset_callback)) {}

ContextualTasksUI::InnerFrameCreationObvserver::~InnerFrameCreationObvserver() =
    default;

void ContextualTasksUI::InnerFrameCreationObvserver::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  CHECK(callback_);
  callback_.Run(inner_web_contents);
}

void ContextualTasksUI::InnerFrameCreationObvserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // If the main frame navigates, reset the embedded page so that the
  // ContextualTaskUI can listen to the a new inner WebContents if needed.
  if (navigation_handle->IsInPrimaryMainFrame() &&
      navigation_handle->HasCommitted() &&
      !navigation_handle->IsSameDocument()) {
    CHECK(reset_callback_);
    reset_callback_.Run();
  }
}

void ContextualTasksUI::ResetEmbeddedPage() {
  embedded_web_contents_ = nullptr;
  nav_observer_.reset();
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
          profile, contextual_tasks_service, optimization_guide_keyed_service,
          std::move(receiver), std::move(page));
}

void ContextualTasksUI::PrepareForTaskChange() {
  auto_suggestion_manager_->Reset();
  if (composebox_handler_) {
    composebox_handler_->ResetInputStateModel();
    composebox_handler_->UpdateSuggestedTabContext(nullptr);
  }
}

void ContextualTasksUI::OnTaskChanged() {
  if (composebox_handler_) {
    composebox_handler_->OnTaskChanged();
  }
  if (!IsShownInTab()) {
    // Update the suggested tab chip.
    OnActiveTabContextStatusChanged();
  }
}

void ContextualTasksUI::UpdateExpandButtonEnabled(bool enabled) {
#if !BUILDFLAG(IS_ANDROID)
  if (page_) {
    page_->SetExpandButtonEnabled(enabled);
  }
#endif
}

void ContextualTasksUI::OnRestoredTabsFetched(
    std::vector<searchbox::mojom::TabInfoPtr> tabs) {
  if (composebox_handler_ &&
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox)) {
    composebox_handler_->SetAimThreadRestoredTabs(std::move(tabs));
  }
}

#if !BUILDFLAG(IS_ANDROID)
// static
// Favicons for WebUI pages are only used on desktop builds.
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
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

void ContextualTasksUI::SyncZoom(bool site_to_webui) {
  if (tracked_zoom_host_.empty()) {
    return;
  }

  content::WebContents* web_contents = web_ui()->GetWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetDefaultForBrowserContext(profile);

  std::string webui_host(web_contents->GetLastCommittedURL().host());
  double webui_zoom =
      zoom_map->GetZoomLevelForHostAndScheme("https", webui_host);
  double site_zoom =
      zoom_map->GetZoomLevelForHostAndScheme("https", tracked_zoom_host_);

  // Prevent infinite loops and handle floating-point precision issues by only
  // updating if the difference is significant.
  if (std::abs(webui_zoom - site_zoom) <= 0.01) {
    return;
  }

  if (site_to_webui) {
    zoom_map->SetZoomLevelForHost(webui_host, site_zoom);
  } else {
    zoom_map->SetZoomLevelForHost(tracked_zoom_host_, webui_zoom);
  }
}

void ContextualTasksUI::UpdateZoom() {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  if (!zoom_controller) {
    zoom::ZoomController::CreateForWebContents(web_contents);
    zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  }

  if (IsShownInTab()) {
    zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_DEFAULT);
    SyncZoom(/*site_to_webui=*/true);
  } else {
    zoom_controller->SetZoomMode(zoom::ZoomController::ZOOM_MODE_DISABLED);
  }
}

void ContextualTasksUI::WebUIPrimaryPageChanged(content::Page& page) {
  ui::MojoWebUIController::WebUIPrimaryPageChanged(page);
  // Update zoom when WebUI is loaded.
  UpdateZoom();
}

void ContextualTasksUI::OnZoomLevelChanged(
    const content::HostZoomMap::ZoomLevelChange& change) {
  if (change.mode != content::HostZoomMap::ZOOM_CHANGED_FOR_HOST) {
    return;
  }

  content::WebContents* web_contents = web_ui()->GetWebContents();
  std::string_view current_host = web_contents->GetLastCommittedURL().host();

  if (change.host == tracked_zoom_host_) {
    UpdateZoom();
  } else if (!tracked_zoom_host_.empty() && !current_host.empty() &&
             change.host == current_host) {
    SyncZoom(/*site_to_webui=*/false);
  }
}

#endif  // !BUILDFLAG(IS_ANDROID)

WEB_UI_CONTROLLER_TYPE_IMPL(ContextualTasksUI)
