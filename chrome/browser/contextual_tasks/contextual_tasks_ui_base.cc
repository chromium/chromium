// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_base.h"

#include <algorithm>
#include <iterator>
#include <optional>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals_page_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_post_rearchitecture.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/account_utils.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_id.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "net/base/url_util.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "ui/base/device_form_factor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/resource_path.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/grit/contextual_tasks_extension_resources.h"
#include "chrome/grit/contextual_tasks_extension_resources_map.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#endif
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"
#include "chrome/grit/webui_toolbar_shared_resources.h"
#include "chrome/grit/webui_toolbar_shared_resources_map.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/feature_engagement/public/feature_constants.h"
#endif

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/grit/guest_view_shared_resources_map.h"  // nogncheck
#endif

namespace contextual_tasks {

namespace {

constexpr char kMyActivityUrl[] = "https://myactivity.google.com/myactivity";
constexpr std::string_view kProfileAvatarPathPrefix =
    "profile-avatar.png?gaia_id=";

void OpenUrlWithDisposition(Profile* profile,
                            const GURL& url,
                            WindowOpenDisposition disposition,
                            BrowserWindowInterface* browser) {
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  params.browser = browser;
  Navigate(&params);
}

GURL GetProfileAvatarUrl(Profile* profile, const GURL& url) {
  if (auto* identity_manager = IdentityManagerFactory::GetForProfile(profile)) {
    // Check if user is signed into Chrome. If so, hide the avatar as it is
    // duplicative with the one in top Chrome.
    if (identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin)) {
      return GURL();
    }
    // If the user is not signed into Chrome, but signed into Google.com,
    // return cached avatar (if available; otherwise return generic).
    std::optional<gaia::ListedAccount> account =
        contextual_tasks::GetAccountFromCookieJar(identity_manager, url);
    if (account.has_value() && !account->signed_out && account->valid) {
      AccountInfo extended_info =
          identity_manager->FindExtendedAccountInfoByGaiaId(account->gaia_id);
      if (!extended_info.IsEmpty() &&
          extended_info.GetAvatarImage().has_value()) {
        return GURL(base::StrCat({chrome::kChromeUIContextualTasksURL,
                                  kProfileAvatarPathPrefix,
                                  account->gaia_id.ToString()}));
      }
      return GURL(profiles::GetPlaceholderAvatarIconUrl());
    }
  }
  // If the user is fully signed out in Google + Chrome, do not show the
  // avatar.
  return GURL();
}

bool IsLegacyOnlyContextualTasksResource(std::string_view path) {
  static constexpr std::string_view kLegacyOnlyPaths[] = {
      "app.css.js",
      "app.html.js",
      "app.js",
      "banner_promo.css.js",
      "banner_promo.html.js",
      "banner_promo.js",
      "composebox.css.js",
      "composebox.html.js",
      "composebox.js",
      "composebox.mojom-webui.js",
      "contextual_tasks.html",
      "contextual_tasks_inner_composebox.css.js",
      "contextual_tasks_inner_composebox.html.js",
      "contextual_tasks_inner_composebox.js",
      "contextual_tasks_inner_composebox_context_menu.html.js",
      "error_dialog.css.js",
      "error_dialog.html.js",
      "error_dialog.js",
      "error_page.css.js",
      "error_page.html.js",
      "error_page.js",
      "info_tooltip.css.js",
      "info_tooltip.html.js",
      "info_tooltip.js",
      "onboarding_tooltip.css.js",
      "onboarding_tooltip.html.js",
      "onboarding_tooltip.js",
      "web_view_type.js",
      "window_manager.js",
  };
  return std::ranges::contains(kLegacyOnlyPaths, path);
}

// `WebUIDataSource::SetRequestFilter` only supports a single filter callback
// pair, so `ShouldHandleContextualTasksRequest` and
// `HandleContextualTasksRequest` multiplex both `WebUIToolbarLayoutCssHelper`
// and `kProfileAvatarPathPrefix` requests.
bool ShouldHandleContextualTasksRequest(const std::string& path) {
#if !BUILDFLAG(IS_ANDROID)
  if (WebUIToolbarLayoutCssHelper::ShouldHandleRequest(path)) {
    return true;
  }
#endif
  return base::StartsWith(path, kProfileAvatarPathPrefix);
}

void HandleContextualTasksRequest(
    const base::WeakPtr<Profile>& profile,
    const std::string& path,
    content::WebUIDataSource::GotDataCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (WebUIToolbarLayoutCssHelper::ShouldHandleRequest(path)) {
    WebUIToolbarLayoutCssHelper::HandleRequest(path, std::move(callback));
    return;
  }
#endif
  scoped_refptr<base::RefCountedMemory> png_bytes;
  if (profile) {
    if (auto* identity_manager =
            IdentityManagerFactory::GetForProfile(profile.get())) {
      GaiaId gaia_id(path.substr(kProfileAvatarPathPrefix.size()));
      AccountInfo extended_info =
          identity_manager->FindExtendedAccountInfoByGaiaId(gaia_id);
      if (!extended_info.IsEmpty() &&
          extended_info.GetAvatarImage().has_value()) {
        png_bytes = extended_info.GetAvatarImage()->As1xPNGBytes();
      }
    }
  }
  std::move(callback).Run(std::move(png_bytes));
}

// Returns true if the AI page at `url` has finished streaming a response.
// Initial AI query navigations commit with `q` and a client `mtid` before the
// response streams in. The turn token (`mstk`, or `smstk` on shared threads)
// is appended via a same-document navigation once streaming completes, so its
// presence is used as the signal that there are active results on the page.
// TODO(crbug.com/578072477): Replace this query param check with an explicit
// post message from AIM once one is available to signal streaming completion.
bool HasActiveAimResults(const GURL& url) {
  return net::GetValueForKeyInQuery(url, "mstk", nullptr) ||
         net::GetValueForKeyInQuery(url, "smstk", nullptr);
}

}  // namespace

ContextualTasksUIBase::ContextualTasksUIBase(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/true,
                              /*enable_chrome_histograms=*/true) {
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = GetProfile();
  if (contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
          profile)) {
    if (auto* model = PinnedToolbarActionsModel::Get(profile)) {
      pinned_toolbar_actions_model_observation_.Observe(model);
    }
  }
#endif

  std::vector<ui::ElementIdentifier> tracked_element_ids = {
      ContextualTasksUI::kSmartTabSharingMenuItemElementId,
      kContextualTasksWebUIPinButtonElementId,
      kContextualTasksWebUIToolbarElementId,
      kContextualTasksWebUIOverflowMenuElementId,
      kContextualTasksWebUIOverflowMenuPinButtonElementId,
      kContextualTasksSuperGButtonElementId,
      // `<contextual-tasks-context-library>` also mounts directly inside
      // `chrome://contextual-tasks` (pre-rearchitecture `app.ts` and WebUI
      // Mocha tests), not only in the component extension subframe.
      kContextualTasksFaviconsElementId};
#if !BUILDFLAG(IS_ANDROID)
  tracked_element_ids.push_back(
      PermissionChipView::kPermissionRequestChipElementId);
  tracked_element_ids.push_back(PermissionChipView::kIndicatorChipElementId);
#endif
  ui::TrackedElementHandlerDocumentSingleton::Register(
      this, std::move(tracked_element_ids));
}

ContextualTasksUIBase::~ContextualTasksUIBase() = default;

Profile* ContextualTasksUIBase::GetProfile() {
  if (!web_ui() || !web_ui()->GetWebContents()) {
    return nullptr;
  }
  return Profile::FromWebUI(web_ui());
}

content::WebContents* ContextualTasksUIBase::GetWebUIWebContents() {
  return web_ui()->GetWebContents();
}

BrowserWindowInterface* ContextualTasksUIBase::GetBrowser() {
  content::WebContents* web_contents = web_ui()->GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  BrowserWindowInterface* window =
      webui::GetBrowserWindowInterface(web_contents);
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

ContextualTasksPanelController* ContextualTasksUIBase::GetPanelController() {
  if (!web_ui()->GetWebContents()) {
    return nullptr;
  }

  auto* browser = webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return nullptr;
  }
  return ContextualTasksPanelController::From(browser);
}

contextual_tasks::ContextualTasksUiService*
ContextualTasksUIBase::GetUiService() {
  Profile* profile = GetProfile();
  return profile
             ? ContextualTasksUiServiceFactory::GetForBrowserContext(profile)
             : nullptr;
}

content::WebUIDataSource* ContextualTasksUIBase::RegisterWebUIDataSource(
    Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIContextualTasksHost);
  if (contextual_tasks::IsContextualTasksSidePanelRearchitectureEnabled()) {
    webui::SetJSModuleDefaults(source);
    webui::EnableTrustedTypesCSP(source);
    std::vector<webui::ResourcePath> resources;
    resources.reserve(std::size(kContextualTasksResources));
    for (const auto& resource : kContextualTasksResources) {
      if (!IsLegacyOnlyContextualTasksResource(resource.path)) {
        resources.push_back(resource);
      }
    }
    source->AddResourcePaths(resources);
  } else {
    webui::SetupWebUIDataSource(source, kContextualTasksResources,
                                IDR_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_HTML);
  }

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' https://*.google.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      "media-src blob: data: 'self';");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      base::StrCat(
          {webui::kDefaultTrustedTypesPolicies, " composebox-input;"}));

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  source->AddResourcePaths(kGuestViewSharedResources);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  source->AddResourcePaths(kContextualTasksExtensionResources);
#endif

#if !BUILDFLAG(IS_ANDROID)
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
  source->AddResourcePaths(kWebuiToolbarSharedResources);
#endif
  source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleContextualTasksRequest),
      base::BindRepeating(&HandleContextualTasksRequest,
                          profile->GetWeakPtr()));

  source->AddResourcePath(
      "internals",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);
  source->AddResourcePath(
      "internals/",
      IDR_CONTEXTUAL_TASKS_INTERNALS_CONTEXTUAL_TASKS_INTERNALS_HTML);

  source->AddLocalizedStrings(GetContextualTasksLoadTimeData(profile));

  return source;
}

base::DictValue ContextualTasksUIBase::GetContextualTasksLoadTimeData(
    Profile* profile) {
  base::DictValue dict;

  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"close", IDS_CLOSE},
      {"closeTooltip", IDS_CONTEXTUAL_TASKS_SIDE_PANEL_CLOSE_TOOL_TIP},
      {"contextTooltip", IDS_CONTEXTUAL_TASKS_SIDE_PANEL_CONTEXT_TOOL_TIP},
      {"continueThread", IDS_CONTEXTUAL_TASKS_CONTINUE_THREAD_MESSAGE},
      {"feedback", IDS_LENS_SEND_FEEDBACK},
      {"help", IDS_CONTEXTUAL_TASKS_MENU_HELP},
      {"learnMore", IDS_LEARN_MORE},
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
  };

  for (const auto& str : kLocalizedStrings) {
    dict.Set(str.name, l10n_util::GetStringUTF16(str.id));
  }

  dict.Set("webuiRoundedIconsAttribute",
           features::IsWebUIRoundedIconsEnabled() ? "webui-rounded-icons" : "");

  bool is_eligible =
      contextual_tasks::EntryPointEligibilityManager::IsEligible(profile);
  dict.Set("isCobrowseEligible", is_eligible);
  const bool is_pinning_eligible =
      contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
          profile);
  dict.Set("enablePinButton", is_pinning_eligible);
  dict.Set("enablePinButtonOnSrp",
           is_pinning_eligible &&
               contextual_tasks::IsContextualTasksPinButtonOnSrpEnabled());
  dict.Set(
      "isSidePanelPinned",
      is_pinning_eligible && contextual_tasks::GetEffectivePinState(profile));
  dict.Set("contextualTasksSidePanelRearchitectureEnabled",
           contextual_tasks::IsContextualTasksSidePanelRearchitectureEnabled());
  dict.Set("hideMenuOnAiPageEnabled",
           base::FeatureList::IsEnabled(
               contextual_tasks::kContextualTasksHideMenuOnAiPage));
  dict.Set(
      "contextualTasksEnableSpatialModelToolbarLayout",
      contextual_tasks::GetContextualTasksSpatialModelToolbarLayoutEnabled());
  dict.Set(
      "contextualTasksEnableSpatialModelToolbarLayoutNewThreadInOverflow",
      contextual_tasks::
          GetContextualTasksSpatialModelToolbarLayoutNewThreadInOverflow());
  dict.Set(
      "contextManagementInComposeboxEnabled",
      base::FeatureList::IsEnabled(omnibox::kContextManagementInComposebox));
  dict.Set(
      "composeboxSkillsEnabled",
      base::FeatureList::IsEnabled(omnibox::kComposeboxSkillsContextualTasks));
  dict.Set("webuiRoundedIconsEnabled", features::IsWebUIRoundedIconsEnabled());

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  bool is_aim_eligible =
      aim_eligibility_service && aim_eligibility_service->IsAimEligible();
  dict.Set("isAimEligible", is_aim_eligible);

  dict.Set("darkMode", contextual_tasks::ShouldUseDarkMode(profile));
  dict.Set("isAiPage", false);
  dict.Set("isSignedIn", false);
  dict.Set("expandButtonEnabled", false);
  dict.Set("profileAvatarUrl", GetProfileAvatarUrl(profile, GURL()).spec());

  dict.Set("isSmallDeviceFormFactor",
           ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_PHONE);
#if BUILDFLAG(IS_ANDROID)
  dict.Set("isUserFeedbackAllowed",
           profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed));
#else
  dict.Set("isUserFeedbackAllowed", true);
#endif
  dict.Set("contextualTasksUnboundedMenuEnabled",
           contextual_tasks::IsContextualTasksUnboundedMenuEnabled());

  return dict;
}

void ContextualTasksUIBase::CreatePageHandler(
    mojo::PendingRemote<contextual_tasks_toolbar::mojom::Page> page,
    mojo::PendingReceiver<contextual_tasks_toolbar::mojom::PageHandler>
        page_handler) {
  toolbar_page_.reset();
  toolbar_page_handler_receiver_.reset();
  toolbar_page_.Bind(std::move(page));
  toolbar_page_handler_receiver_.Bind(std::move(page_handler));
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = GetProfile();
  if (contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
          profile)) {
    if (auto* model = PinnedToolbarActionsModel::Get(profile)) {
      if (!pinned_toolbar_actions_model_observation_.IsObserving()) {
        pinned_toolbar_actions_model_observation_.Observe(model);
      }
      bool is_pinned = contextual_tasks::GetEffectivePinState(profile);
      if (auto* toolbar_page = GetToolbarPageRemote()) {
        toolbar_page->OnSidePanelPinStateChanged(is_pinned);
      }
    }
  }
#endif
  if (IsAiPage()) {
    NotifyAiPageStatusChanged(true);
  }
  if (GetThreadTitle().has_value() && toolbar_page_) {
    toolbar_page_->SetThreadTitle(*GetThreadTitle());
  }

  UpdateProfileIndicator();
}

void ContextualTasksUIBase::PinSidePanel() {
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = GetProfile();
  if (!contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
          profile)) {
    return;
  }
  if (auto* model = PinnedToolbarActionsModel::Get(profile)) {
    model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);
  }
#endif
}

void ContextualTasksUIBase::UnpinSidePanel() {
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = GetProfile();
  if (!contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
          profile)) {
    return;
  }
  if (auto* model = PinnedToolbarActionsModel::Get(profile)) {
    model->UpdatePinnedState(kActionSidePanelShowContextualTasks, false);
  }
#endif
}

void ContextualTasksUIBase::OpenMyActivityUi() {
  BrowserWindowInterface* browser = GetBrowser();
  if (!browser) {
    return;
  }
  OpenUrlWithDisposition(GetProfile(), GURL(kMyActivityUrl),
                         WindowOpenDisposition::kNewForegroundTab, browser);
}

void ContextualTasksUIBase::OpenOverflowMenuHelpUi() {
  BrowserWindowInterface* browser = GetBrowser();
  if (!browser) {
    return;
  }
  OpenUrlWithDisposition(
      GetProfile(),
      GURL(contextual_tasks::GetContextualTasksOverflowMenuHelpUrl()),
      WindowOpenDisposition::kNewForegroundTab, browser);
}

void ContextualTasksUIBase::OpenFeedbackUi() {
  BrowserWindowInterface* browser = GetBrowser();
  if (!browser) {
    return;
  }
  content::WebContents* web_contents = GetWebUIWebContents();
  GURL page_url = web_contents ? web_contents->GetLastCommittedURL() : GURL();
  if (auto* tab_list = TabListInterface::From(browser)) {
    if (auto* active_tab = tab_list->GetActiveTab()) {
      if (active_tab->GetContents()) {
        page_url = active_tab->GetContents()->GetLastCommittedURL();
      }
    }
  }

  if (auto* ui_service =
          ContextualTasksUiServiceFactory::GetForBrowserContext(GetProfile())) {
    ui_service->OpenFeedbackUi(browser, page_url);
  }
}

void ContextualTasksUIBase::MoveTaskUiToNewTab() {
  // Base implementation is a no-op; overridden by ContextualTasksUI.
}

void ContextualTasksUIBase::ShowThreadHistory() {
#if BUILDFLAG(ENABLE_EXTENSIONS_CORE) && !BUILDFLAG(IS_ANDROID)
  lens::ClientToSearchMessage message;
  message.mutable_open_threads_view();

  if (auto* panel_controller = GetPanelController()) {
    if (auto* active_contents = panel_controller->GetActiveWebContents()) {
      if (auto* user_data = ContextualTasksWebContentsUserData::FromWebContents(
              active_contents)) {
        user_data->PostSearchMessage(message);
      }
    }
  }
#endif
}

#if !BUILDFLAG(IS_ANDROID)
void ContextualTasksUIBase::OnActionsChanged() {
  if (auto* toolbar_page = GetToolbarPageRemote()) {
    Profile* profile = GetProfile();
    bool effective_pin_state =
        contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
            profile) &&
        contextual_tasks::GetEffectivePinState(profile);
    toolbar_page->OnSidePanelPinStateChanged(effective_pin_state);
  }
}
#endif

void ContextualTasksUIBase::OnTabClickedFromSourcesMenu(int32_t tab_id,
                                                        const GURL& url) {
  auto* ui_service = GetUiService();
  if (ui_service) {
    ui_service->OnTabClickedFromSourcesMenu(tab_id, url, GetBrowser());
  }
}

void ContextualTasksUIBase::OnFileClickedFromSourcesMenu(const GURL& url) {
  auto* ui_service = GetUiService();
  if (ui_service) {
    ui_service->OnFileClickedFromSourcesMenu(url, GetBrowser());
  }
}

void ContextualTasksUIBase::OnImageClickedFromSourcesMenu(const GURL& url) {
  auto* ui_service = GetUiService();
  if (ui_service) {
    ui_service->OnImageClickedFromSourcesMenu(url, GetBrowser());
  }
}

void ContextualTasksUIBase::CloseSidePanel() {
  auto* controller = GetPanelController();
  if (!controller) {
    return;
  }
  controller->Close();
}

void ContextualTasksUIBase::ShowPageInfoBubble(bool is_pointer_interaction) {
  auto* controller = GetPanelController();
  if (!controller) {
    return;
  }
  controller->ShowPageInfoBubble(is_pointer_interaction);
}

void ContextualTasksUIBase::OnLogoPointerDown() {
  auto* controller = GetPanelController();
  if (!controller) {
    return;
  }
  controller->OnLogoPointerDown();
}

void ContextualTasksUIBase::CreateNewThread() {
  auto* panel_controller = GetPanelController();
  if (!panel_controller) {
    return;
  }
  content::WebContents* target_contents =
      panel_controller->GetActiveWebContents();
  if (!target_contents) {
    return;
  }

  // When scoped to the tab, start the new thread in a new task for the active
  // tab only, so that other tabs sharing the task keep their thread.
  if (contextual_tasks::ShouldScopeSidePanelStateToTab()) {
    if (GetWebUIWebContents() != panel_controller->GetToolbarWebContents()) {
      return;
    }
    if (panel_controller->MoveActiveTabToNewThreadIfShared()) {
      SetThreadTitle(std::nullopt);
      return;
    }
  }

  std::optional<base::Uuid> task_id;
  if (auto current_task = panel_controller->GetCurrentTask()) {
    task_id = current_task->GetTaskId();
  } else if (auto* helper = ContextualSearchWebContentsHelper::FromWebContents(
                 target_contents)) {
    task_id = helper->task_id();
  }

  auto* ui_service = GetUiService();
  if (!ui_service) {
    return;
  }

  GURL url = task_id.has_value()
                 ? ui_service->GetDefaultAiPageUrlForTask(task_id.value())
                 : ui_service->GetDefaultAiPageUrl();
  url = ui_service->AddRequiredSidePanelUrlChanges(url, target_contents);

  content::NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  target_contents->GetController().LoadURLWithParams(params);

  SetThreadTitle(std::nullopt);
}

// Pinning IPH trigger criteria: The pinning IPH should try to trigger when:
// 1. Results have fully loaded for a contextual query about a tab or PDF
// 2. The side panel is open & the ephemeral button is showing
// 3. The user hasn't yet pinned the side panel
bool ContextualTasksUIBase::CanTriggerPinningPromoForAiPage(
    const GURL& url,
    ContextualTasksPanelController* panel_controller) {
  // 1a. Results have fully loaded (finished streaming).
  if (!HasActiveAimResults(url) || !panel_controller) {
    return false;
  }

  // 1b. Query has tab or PDF context.
  const bool is_lens_overlay_entry =
      panel_controller->GetActiveEntrySource() ==
      ContextualTasksPanelController::EntrySource::kLensOverlay;
  // Server-committed resources on the task (skipped for Lens Overlay, which
  // includes the implicit initial Lens upload).
  if (!is_lens_overlay_entry) {
    std::optional<ContextualTask> current_task =
        panel_controller->GetCurrentTask();
    if (current_task &&
        std::ranges::any_of(
            current_task->GetUrlResources(), [](const UrlResource& r) {
              return r.resource_type == ResourceType::kPdf ||
                     r.resource_type == ResourceType::kWebpage ||
                     r.tab_id.has_value() || r.has_chrome_tab_data;
            })) {
      return true;
    }
  }

  // Fallback: client-side composebox tab/PDF context (before `ContextualTask`
  // updates, or when manually added in a Lens Overlay session).
  contextual_search::ContextualSearchSessionHandle* session_handle =
      panel_controller->GetContextualSearchSessionHandleForPanel();
  if (!session_handle) {
    return false;
  }

  const contextual_search::TabContextState& tab_context =
      session_handle->GetTabContextState();
  if ((!is_lens_overlay_entry && !tab_context.restored.empty()) ||
      std::ranges::any_of(
          tab_context.attached,
          [is_lens_overlay_entry](const contextual_search::TabInfo& tab) {
            return !is_lens_overlay_entry || tab.uploaded ||
                   tab.pending_delayed_upload;
          })) {
    return true;
  }

  return session_handle->HasPdfContext();
}

void ContextualTasksUIBase::MaybeTriggerPinningPromo() {
#if !BUILDFLAG(IS_ANDROID)
  // Side panel is open & pinning/ephemeral button eligibility is active.
  auto* panel_controller = GetPanelController();
  if (!panel_controller || !panel_controller->IsPanelOpenForContextualTask() ||
      panel_controller->IsOnboardingTooltipVisible()) {
    return;
  }

  Profile* profile = GetProfile();
  if (!profile) {
    return;
  }

  if (!contextual_tasks::IsContextualTasksPinningV2Enabled() ||
      !contextual_tasks::EntryPointEligibilityManager::IsPinningEligible(
          profile)) {
    return;
  }

  // Results have fully loaded for a contextual query about a tab or PDF.
  if (!IsAiPage()) {
    return;
  }

  auto* active_contents = panel_controller->GetActiveWebContents();
  if (!active_contents ||
      !CanTriggerPinningPromoForAiPage(active_contents->GetLastCommittedURL(),
                                       panel_controller)) {
    return;
  }

  // User hasn't yet pinned the side panel.
  if (contextual_tasks::GetEffectivePinState(profile)) {
    return;
  }

  int post_onboarding_sessions = profile->GetPrefs()->GetInteger(
      contextual_tasks::kContextualTasksSessionCountPostOnboarding);
  if (post_onboarding_sessions <
      contextual_tasks::GetContextualTasksNumSessionsBeforeRequestPinPromo()) {
    return;
  }

  // Anchored to `kContextualTasksEphemeralToolbarButtonElementId`,
  // so it only shows if the ephemeral button is visible.
  BrowserWindowInterface* browser_window = GetBrowser();
  if (!browser_window) {
    return;
  }
  BrowserUserEducationInterface::From(browser_window)
      ->MaybeShowFeaturePromo(
          feature_engagement::kIPHSidePanelContextualTasksPinnableFeature);
#endif
}

void ContextualTasksUIBase::BindInterface(
    mojo::PendingReceiver<contextual_tasks_toolbar::mojom::PageHandlerFactory>
        pending_receiver) {
  toolbar_page_factory_receiver_.reset();
  toolbar_page_factory_receiver_.Bind(std::move(pending_receiver));
}

void ContextualTasksUIBase::BindInterface(
    mojo::PendingReceiver<
        contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIService>
        pending_receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(pending_receiver));
}

void ContextualTasksUIBase::BindInterface(
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandlerFactory>
        pending_receiver) {
  help_bubble_factory_receiver_.reset();
  help_bubble_factory_receiver_.Bind(std::move(pending_receiver));
}

void ContextualTasksUIBase::BindInterface(
    mojo::PendingReceiver<contextual_tasks_internals::mojom::
                              ContextualTasksInternalsPageHandlerFactory>
        pending_receiver) {
  contextual_tasks_internals_page_handler_receiver_.reset();
  contextual_tasks_internals_page_handler_receiver_.Bind(
      std::move(pending_receiver));
}

void ContextualTasksUIBase::CreatePageHandler(
    mojo::PendingRemote<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPage> page,
    mojo::PendingReceiver<
        contextual_tasks_internals::mojom::ContextualTasksInternalsPageHandler>
        receiver) {
  Profile* profile = GetProfile();
  auto* context_service =
      profile ? ContextualTasksContextServiceFactory::GetForProfile(profile)
              : nullptr;
  auto* optimization_guide_keyed_service =
      profile ? OptimizationGuideKeyedServiceFactory::GetForProfile(profile)
              : nullptr;
  contextual_tasks_internals_page_handler_ =
      std::make_unique<ContextualTasksInternalsPageHandler>(
          profile, context_service, optimization_guide_keyed_service,
          std::move(receiver), std::move(page));
}

void ContextualTasksUIBase::CreateHelpBubbleHandler(
    mojo::PendingRemote<help_bubble::mojom::HelpBubbleClient> client,
    mojo::PendingReceiver<help_bubble::mojom::HelpBubbleHandler> handler) {
  help_bubble_handler_ = std::make_unique<user_education::HelpBubbleHandler>(
      std::move(handler), std::move(client),
      ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
          web_ui()->GetRenderFrameHost()));
}

ContextualTasksPermissionController*
ContextualTasksUIBase::GetActiveController() {
  BrowserWindowInterface* browser =
      webui::GetBrowserWindowInterface(web_ui()->GetWebContents());
  if (!browser) {
    return nullptr;
  }
  auto* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser);
  return controller ? controller->permission_controller() : nullptr;
}

ContextualTasksUIBase* ContextualTasksUIBase::FromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  content::WebUI* web_ui = web_contents->GetWebUI();
  if (!web_ui) {
    return nullptr;
  }
  content::WebUIController* controller = web_ui->GetController();
  // `GetAs()` CHECKs if the controller has no WEB_UI_CONTROLLER_TYPE_DECL().
  if (!controller || !controller->GetType()) {
    return nullptr;
  }
  // `GetAs()` matches the exact concrete type, so every `ContextualTasksUIBase`
  // subclass has to be listed here.
  if (auto* ui = controller->GetAs<ContextualTasksUI>()) {
    return ui;
  }
  return controller->GetAs<ContextualTasksUIPostRearchitecture>();
}

void ContextualTasksUIBase::NotifyPermissionDashboardStateChanged(
    ContextualTasksPermissionController* controller) {
  // All tasks in a window share one controller and dashboard, which shows
  // the active task. Each task's permission data stays on its WebContents and
  // is re-read on a task switch. Drop updates from any other controller.
  if (!controller || controller != GetActiveController()) {
    return;
  }

  toolbar_ui_api::mojom::PermissionDashboardStatePtr state =
      controller->GetState();
  if (mojo::Equals(state, last_pushed_permission_dashboard_state_)) {
    return;
  }
  last_pushed_permission_dashboard_state_ = state.Clone();

  // Notify observers (one of which is the webUI).
  for (auto& observer : toolbar_ui_observers_) {
    observer->OnPermissionDashboardStateChanged(state.Clone());
  }
}

void ContextualTasksUIBase::SetIsAiPage(bool is_ai_page) {
  is_ai_page_ = is_ai_page;
  NotifyAiPageStatusChanged(is_ai_page);
}

const std::optional<std::string>& ContextualTasksUIBase::GetThreadTitle() {
  return thread_title_;
}

void ContextualTasksUIBase::SetThreadTitle(std::optional<std::string> title) {
  thread_title_ = std::move(title);
  if (auto* toolbar_page = GetToolbarPageRemote()) {
    toolbar_page->SetThreadTitle(thread_title_.value_or(std::string()));
  }
}

void ContextualTasksUIBase::NotifySidePanelStateChanged() {
  if (auto* toolbar_page = GetToolbarPageRemote()) {
    toolbar_page->OnSidePanelStateChanged();
  }
}

void ContextualTasksUIBase::GetInitialState(GetInitialStateCallback callback) {
  auto* controller = GetActiveController();
  if (!controller) {
    std::move(callback).Run(base::unexpected(mojo_base::mojom::Error::New(
        mojo_base::mojom::Code::kFailedPrecondition,
        "ContextualTasksToolbarUIService: no active controller")));
    return;
  }

  auto initial_state = contextual_tasks_toolbar::mojom::InitialState::New();
  mojo::PendingRemote<
      contextual_tasks_toolbar::mojom::ContextualTasksToolbarUIObserver>
      observer_remote;
  initial_state->update_stream =
      observer_remote.InitWithNewPipeAndPassReceiver();
  toolbar_ui_observers_.Add(std::move(observer_remote));

  initial_state->state = controller->GetState();
  last_pushed_permission_dashboard_state_ = initial_state->state.Clone();

  std::move(callback).Run(std::move(initial_state));
}

void ContextualTasksUIBase::OnChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_mouse_interaction,
    uint32_t state_token) {
  if (auto* controller = GetActiveController()) {
    controller->OnChipClicked(identifier, is_mouse_interaction, state_token);
  }
}

void ContextualTasksUIBase::OnChipExpandAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (auto* controller = GetActiveController()) {
    controller->OnChipExpandAnimationEnded(identifier);
  }
}

void ContextualTasksUIBase::OnChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (auto* controller = GetActiveController()) {
    controller->OnChipCollapseAnimationEnded(identifier);
  }
}

void ContextualTasksUIBase::OnChipMousePressed(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {}

void ContextualTasksUIBase::OnChipPointerEntered(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (ContextualTasksPermissionController* controller = GetActiveController()) {
    controller->OnChipPointerEntered(identifier);
  }
}

void ContextualTasksUIBase::OnChipPointerExited(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (ContextualTasksPermissionController* controller = GetActiveController()) {
    controller->OnChipPointerExited(identifier);
  }
}

void ContextualTasksUIBase::NotifyAiPageStatusChanged(bool is_ai_page) {
  if (auto* toolbar_page = GetToolbarPageRemote()) {
    toolbar_page->OnAiPageStatusChanged(is_ai_page);
  }
}

void ContextualTasksUIBase::UpdateProfileIndicator() {
  GURL active_url;
  if (auto* panel = GetPanelController()) {
    if (auto* contents = panel->GetActiveWebContents()) {
      active_url = contents->GetVisibleURL();
    }
  }
  UpdateProfileIndicator(active_url);
}

void ContextualTasksUIBase::UpdateProfileIndicator(const GURL& url) {
  if (auto* toolbar_page = GetToolbarPageRemote()) {
    toolbar_page->SetProfileAvatarUrl(GetProfileAvatarUrl(GetProfile(), url));
  }
}

}  // namespace contextual_tasks
