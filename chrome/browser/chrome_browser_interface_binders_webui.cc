// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders_webui.h"

#include "build/android_buildflags.h"
#include "chrome/browser/chrome_browser_interface_binders.h"
#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals.mojom.h"
#include "chrome/browser/ui/webui/chrome_finds_internals/chrome_finds_internals_ui.h"
#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_ui.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom.h"
#include "chrome/browser/ui/webui/context_hub/context_hub_ui.h"
#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/location_internals/location_internals.mojom.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/omnibox/aim_eligibility/aim_eligibility.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals.mojom.h"
#include "chrome/browser/ui/webui/personal_context_internals/personal_context_internals_ui.h"
#include "chrome/browser/ui/webui/policy/policy_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/subresource_filter/subresource_filter_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/actor/public/mojom/actor_internals.mojom.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/internals/mojom/commerce_internals.mojom.h"
#include "components/contextual_tasks/public/features.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/policy/core/common/features.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/message.h"
#include "ui/webui/buildflags.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/indigo_internals/indigo_internals.mojom.h"
#include "chrome/browser/ui/webui/indigo_internals/indigo_internals_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup.mojom.h"
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup_aim.mojom.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals.mojom.h"
#include "chrome/browser/ui/webui/webnn_internals/webnn_internals_ui.h"
#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#endif
#endif

#if BUILDFLAG(ENABLE_WEBUI_NTP)
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "components/search/ntp_features.h"
#include "ui/webui/resources/cr_components/help_bubble/help_bubble.mojom.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX)
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#endif

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#endif
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/skills/skills.mojom.h"
#include "chrome/browser/ui/webui/skills/skills_ui.h"
#endif

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

namespace {
#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
void BindColorChangeListener(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  auto* color_change_handler =
      ui::ColorChangeHandler::GetOrCreateForCurrentDocument(frame_host);
  color_change_handler->Bind(std::move(pending_receiver));
}
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)

void BindTrackedElementHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        pending_receiver) {
  auto handler =
      ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(frame_host);
  if (handler) {
    handler->BindInterface(std::move(pending_receiver));
  }
}

void BindTrackedElementHandlerRestricted(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        pending_receiver) {
  auto* web_ui = frame_host->GetWebUI();
  if (!web_ui) {
    mojo::ReportBadMessage(
        "TrackedElementHandler requested by non-WebUI frame.");
    return;
  }
  auto* controller = web_ui->GetController();
  DCHECK(controller);

  const bool is_allowed =
#if BUILDFLAG(ENABLE_WEBUI_NTP)
      controller->GetAs<NewTabPageUI>() ||
#endif
#if !BUILDFLAG(IS_ANDROID)
      controller->GetAs<UserEducationInternalsUI>() ||
      controller->GetAs<ReadingListUI>() ||
      controller->GetAs<CustomizeChromeUI>() ||
      controller->GetAs<PasswordManagerUI>() ||
      controller->GetAs<HistoryUI>() ||
#if !BUILDFLAG(IS_CHROMEOS)
      controller->GetAs<ProfilePickerUI>() ||
#endif  // !BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_ANDROID)
      controller->GetAs<ContextualTasksUI>();

  if (!is_allowed) {
    mojo::ReportBadMessage(
        "TrackedElementHandler requested by unauthorized WebUI.");
    return;
  }

  BindTrackedElementHandler(frame_host, std::move(pending_receiver));
}

void PopulateChromeWebUIFrameBindersPartsAllPlatforms(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<
      chrome_finds_internals::mojom::PageHandlerFactory,
      chrome_finds_internals::ChromeFindsInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::BluetoothInternalsHandler,
                                         BluetoothInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      media::mojom::MediaEngagementScoreDetailsProvider, MediaEngagementUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<browsing_topics::mojom::PageHandler,
                                         BrowsingTopicsInternalsUI>(map);
#if !BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<
      omnibox_popup_aim::mojom::PageHandlerFactory, OmniboxPopupUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      omnibox_popup::mojom::PageHandlerFactory, OmniboxPopupUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      indigo_internals::mojom::PageHandlerFactory, IndigoInternalsUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      webnn_internals::mojom::PageHandlerFactory, WebNNInternalsUI>(map);
#endif
  RegisterWebUIControllerInterfaceBinder<::mojom::OmniboxPageHandler,
                                         OmniboxUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      aim_eligibility::mojom::PageHandlerFactory, OmniboxUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      site_engagement::mojom::SiteEngagementDetailsProvider, SiteEngagementUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<::mojom::UsbInternalsPageHandler,
                                         UsbInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      history_clusters_internals::mojom::PageHandlerFactory,
      HistoryClustersInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      optimization_guide_internals::mojom::PageHandlerFactory,
      OptimizationGuideInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      segmentation_internals::mojom::PageHandlerFactory,
      SegmentationInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      commerce::mojom::CommerceInternalsHandlerFactory,
      commerce::CommerceInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<chrome_urls::mojom::PageHandlerFactory,
                                         chrome_urls::ChromeUrlsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      data_sharing_internals::mojom::PageHandlerFactory,
      DataSharingInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      connectors_internals::mojom::PageHandler,
      enterprise_connectors::ConnectorsInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::LocationInternalsHandler,
                                         LocationInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      actor_internals::mojom::PageHandlerFactory, ActorInternalsUI>(map);

  if (base::FeatureList::IsEnabled(
          policy::features::kPolicyPageMojoMigration)) {
    RegisterWebUIControllerInterfaceBinder<
        policy::mojom::PolicyPageHandlerFactory, PolicyUI>(map);
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks)) {
    RegisterWebUIControllerInterfaceBinder<
        contextual_tasks::mojom::PageHandlerFactory, ContextualTasksUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      subresource_filter::mojom::SubresourceFilterInternalsHandler,
      subresource_filter::SubresourceFilterInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      browser::context_hub::mojom::PageHandlerFactory, ContextHubUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      browser::personal_context_internals::mojom::PageHandlerFactory,
      PersonalContextInternalsUI>(map);

#if BUILDFLAG(ENABLE_WEBUI_NTP)
  content::RegisterWebUIControllerInterfaceBinder<
      new_tab_page::mojom::PageHandlerFactory, NewTabPageUI>(map);
  content::RegisterWebUIControllerInterfaceBinder<
      most_visited::mojom::MostVisitedPageHandlerFactory, NewTabPageUI>(map);
  content::RegisterWebUIControllerInterfaceBinder<
      customize_buttons::mojom::CustomizeButtonsHandlerFactory, NewTabPageUI>(
      map);
  if (base::FeatureList::IsEnabled(ntp_features::kNtpNextFeatures)) {
    content::RegisterWebUIControllerInterfaceBinder<
        action_chips::mojom::ActionChipsHandlerFactory, NewTabPageUI>(map);
  }
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP)

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
  map->Add<color_change_listener::mojom::PageHandler>(
      base::BindRepeating(&BindColorChangeListener));
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)

// TODO(b/502297163): Implement for Android.
#if BUILDFLAG(ENABLE_WEBUI_NTP) && BUILDFLAG(IS_ANDROID)
  // A variant of these exist in
  // chrome_browser_interface_binders_webui_parts_desktop.cc:
  // that enables them for more pages.
  content::RegisterWebUIControllerInterfaceBinder<
      searchbox::mojom::PageHandlerFactory, NewTabPageUI>(map);
  content::RegisterWebUIControllerInterfaceBinder<
      help_bubble::mojom::HelpBubbleHandlerFactory, NewTabPageUI>(map);
#endif  // BUILDFLAG(ENABLE_WEBUI_NTP) && BUILDFLAG(IS_ANDROID)

// For the case that's !IS_ANDROID, PageHandlerFactory is registered in
// chrome_browser_interface_binders_webui_parts_desktop.cc.
#if BUILDFLAG(IS_ANDROID) && \
    (BUILDFLAG(ENABLE_WEBUI_NTP) || \
     BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX))
  RegisterWebUIControllerInterfaceBinder<composebox::mojom::PageHandlerFactory
#if BUILDFLAG(ENABLE_WEBUI_NTP)
                                         ,
                                         NewTabPageUI
#endif
#if BUILDFLAG(ENABLE_WEBUI_CONTEXTUAL_TASKS_COMPOSEBOX)
                                         ,
                                         ContextualTasksUI
#endif
                                         >(map);
#endif  // BUILDFLAG(IS_ANDROID)

  map->Add<tracked_element::mojom::TrackedElementHandler>(
      base::BindRepeating(&BindTrackedElementHandlerRestricted));

  // End of PopulateChromeWebUIFrameBindersPartsAllPlatforms().
  // Please do not add platform-specific logic to this function.
}

}  // namespace

void PopulateChromeWebUIFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  PopulateChromeWebUIFrameBindersPartsAllPlatforms(map, render_frame_host);
  PopulateChromeWebUIFrameBindersPartsFeatures(map, render_frame_host);

#if BUILDFLAG(IS_ANDROID)
  PopulateChromeWebUIFrameBindersPartsAndroid(map, render_frame_host);
#else
  PopulateChromeWebUIFrameBindersPartsDesktop(map, render_frame_host);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
  PopulateChromeWebUIFrameBindersPartsCros(map, render_frame_host);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
  RegisterWebUIControllerInterfaceBinder<discards::mojom::DetailsProvider,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::GraphDump,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::SiteDataProvider,
                                         DiscardsUI>(map);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<skills::mojom::PageHandlerFactory,
                                         skills::SkillsUI>(map);
#endif

  // When possible, please one one of the Parts functions above and avoid making
  // this function longer.
}

void PopulateTrustedChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // This function is broken up into sections based on WebUI types.

#if BUILDFLAG(IS_CHROMEOS)
  PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsCros(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
  registry.AddGlobal<color_change_listener::mojom::PageHandler>(
      base::BindRepeating(&BindColorChangeListener));
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)

  registry.AddGlobal<tracked_element::mojom::TrackedElementHandler>(
      base::BindRepeating(&BindTrackedElementHandler));

  // When possible, please use one of the Parts functions above and avoid making
  // this function longer.
}

void PopulateUntrustedChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsFeatures(registry);

#if BUILDFLAG(IS_CHROMEOS)
  PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsCros(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
  registry.AddGlobal<color_change_listener::mojom::PageHandler>(
      base::BindRepeating(&BindColorChangeListener));
#endif  // !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)

  registry.AddGlobal<tracked_element::mojom::TrackedElementHandler>(
      base::BindRepeating(&BindTrackedElementHandler));

  // When possible, please use one of the Parts functions above and avoid making
  // this function longer.
}

}  // namespace chrome::internal
