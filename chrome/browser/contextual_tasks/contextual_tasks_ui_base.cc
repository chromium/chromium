// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui_base.h"

#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/entry_point_eligibility_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/contextual_tasks_resources.h"
#include "chrome/grit/contextual_tasks_resources_map.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/grit/contextual_tasks_extension_resources.h"
#include "chrome/grit/contextual_tasks_extension_resources_map.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_layout_css_helper.h"
#include "chrome/grit/webui_toolbar_shared_resources.h"
#include "chrome/grit/webui_toolbar_shared_resources_map.h"
#endif

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
#include "chrome/grit/guest_view_shared_resources_map.h"  // nogncheck
#endif

namespace contextual_tasks {

ContextualTasksUIBase::ContextualTasksUIBase(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui,
                              /*enable_chrome_send=*/true,
                              /*enable_chrome_histograms=*/true) {}

ContextualTasksUIBase::~ContextualTasksUIBase() = default;

Profile* ContextualTasksUIBase::GetProfile() {
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

content::WebUIDataSource* ContextualTasksUIBase::RegisterWebUIDataSource(
    Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUIContextualTasksHost);
  webui::SetupWebUIDataSource(source, kContextualTasksResources,
                              IDR_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_HTML);

  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::ChildSrc,
      "child-src 'self' https://*.google.com;");
  source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::MediaSrc,
      "media-src blob: data: 'self';");

#if !BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  source->AddResourcePaths(kGuestViewSharedResources);
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
  source->AddResourcePaths(kContextualTasksExtensionResources);
#endif

#if !BUILDFLAG(IS_ANDROID)
  source->AddResourcePaths(kWebuiToolbarSharedResources);
  WebUIToolbarLayoutCssHelper::SetAsRequestFilter(source);
#endif

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
  dict.Set("webuiRoundedIconsEnabled", features::IsWebUIRoundedIconsEnabled());

  AimEligibilityService* aim_eligibility_service =
      AimEligibilityServiceFactory::GetForProfile(profile);
  bool is_aim_eligible =
      aim_eligibility_service && aim_eligibility_service->IsAimEligible();
  dict.Set("isAimEligible", is_aim_eligible);

  dict.Set("darkMode", false);
  dict.Set("isAiPage", false);
  dict.Set("isSignedIn", false);
  dict.Set("expandButtonEnabled", false);

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
  auto* coordinator =
      contextual_tasks::ContextualTasksSidePanelCoordinator::Get(
          browser->GetUnownedUserDataHost());
  if (!coordinator) {
    return nullptr;
  }
  content::WebContents* main_contents = coordinator->GetActiveWebContents();
  return main_contents ? contextual_tasks::ContextualTasksPermissionController::
                             FromWebContents(main_contents)
                       : nullptr;
}

// TODO(crbug.com/558849041): Observe ContextualTasksPermissionController and
// active task changes, and dashboard push state updates via
// toolbar_ui_observers_.Notify(
//    &ContextualTasksToolbarUIObserver::OnPermissionDashboardStateChanged).
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

  std::move(callback).Run(std::move(initial_state));
}

// TODO(crbug.com/558848727): Fix race condition where active task changes while
// IPC is in flight. The WebUI should pass the target task ID (or generation
// token), and the click should be dropped if the targeted task is no longer
// active.
void ContextualTasksUIBase::OnChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_mouse_interaction) {
  if (auto* controller = GetActiveController()) {
    controller->OnChipClicked(identifier, is_mouse_interaction);
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
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {}

void ContextualTasksUIBase::OnChipPointerExited(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {}

}  // namespace contextual_tasks
