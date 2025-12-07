// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders_webui.h"

#include "build/android_buildflags.h"
#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_ui.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"
#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/location_internals/location_internals.mojom.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/omnibox_popup/mojom/omnibox_popup_aim.mojom.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#endif
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/internals/mojom/commerce_internals.mojom.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#endif

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

namespace {

void PopulateChromeWebUIFrameBindersPartsAllPlatforms(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
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
#endif
  RegisterWebUIControllerInterfaceBinder<::mojom::OmniboxPageHandler,
                                         OmniboxUI>(map);

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
      privacy_sandbox_internals::mojom::PageHandler,
      privacy_sandbox_internals::PrivacySandboxInternalsUI>(map);

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

  // When possible, please one one of the Parts functions above and avoid making
  // this function longer.
}

void PopulateChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // This function is broken up into sections based on WebUI types.

  // --- Section 1: chrome:// WebUIs:
#if BUILDFLAG(IS_CHROMEOS)
  PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsCros(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

  // --- Section 2: chrome-untrusted:// WebUIs:
  PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsFeatures(registry);

#if BUILDFLAG(IS_CHROMEOS)
  PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsCros(registry);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_ANDROID)
  PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop(registry);
  PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop(registry);
#endif  // !BUILDFLAG(IS_ANDROID)

  // When possible, please one one of the Parts functions above and avoid making
  // this function longer.
}

}  // namespace chrome::internal
