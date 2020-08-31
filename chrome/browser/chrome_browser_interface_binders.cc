// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include <utility>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/engagement/site_engagement_details.mojom.h"
#include "chrome/browser/language/translate_frame_binder.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/predictors/network_hints_handler_impl.h"
#include "chrome/browser/prerender/chrome_prerender_contents_delegate.h"
#include "chrome/browser/prerender/chrome_prerender_processor_impl_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/insecure_sensitive_input_driver_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/internals/internals_ui.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals.mojom.h"
#include "chrome/browser/ui/webui/interventions_internals/interventions_internals_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/media_history_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/contextual_search/buildflags.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/feed/buildflags.h"
#include "components/performance_manager/embedder/binders.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/prefs/pref_service.h"
#include "components/prerender/browser/prerender_contents.h"
#include "components/prerender/browser/prerender_processor_impl.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/mojom/credentialmanager/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/insecure_input/insecure_input_service.mojom.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "third_party/blink/public/public_buildflags.h"

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#endif  // BUILDFLAG(ENABLE_FEED_IN_CHROME)

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "chrome/browser/android/contextualsearch/unhandled_tap_notifier_impl.h"
#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if defined(OS_ANDROID)
#include "chrome/browser/android/contextualsearch/contextual_search_observer.h"
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals_ui.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals.mojom.h"
#include "chrome/browser/ui/webui/snippets_internals/snippets_internals_ui.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/contextual_search/content/browser/contextual_search_js_api_service_impl.h"
#include "components/contextual_search/content/common/mojom/contextual_search_js_api_service.mojom.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#else
#include "chrome/browser/accessibility/caption_host_impl.h"
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/media/feeds/media_feeds_store.mojom.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_data_provider_impl.h"
#include "chrome/browser/media/kaleidoscope/kaleidoscope_ui.h"
#include "chrome/browser/media/kaleidoscope/mojom/kaleidoscope.mojom.h"
#include "chrome/browser/payments/payment_credential_factory.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "chrome/browser/promo_browser_command/promo_browser_command.mojom.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/media/media_feeds_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/read_later/read_later.mojom.h"
#include "chrome/browser/ui/webui/read_later/read_later_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/common/caption.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#endif

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/chromeos/file_manager/file_manager.mojom.h"
#include "chrome/browser/ui/webui/chromeos/file_manager/file_manager_ui.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.mojom.h"
#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_ui.h"
#include "chrome/browser/ui/webui/internals/web_app/web_app_internals.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/user_action_recorder.mojom.h"
#include "chromeos/components/camera_app_ui/camera_app_helper.mojom.h"
#include "chromeos/components/camera_app_ui/camera_app_ui.h"
#include "chromeos/components/help_app_ui/help_app_ui.h"
#include "chromeos/components/help_app_ui/help_app_ui.mojom.h"
#include "chromeos/components/media_app_ui/media_app_ui.h"
#include "chromeos/components/media_app_ui/media_app_ui.mojom.h"
#include "chromeos/components/multidevice/debug_webui/proximity_auth_ui.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"
#include "chromeos/components/print_management/print_management_ui.h"
#include "chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "chromeos/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#endif

#if defined(OS_CHROMEOS) && !defined(OFFICIAL_BUILD)
#include "chromeos/components/telemetry_extension_ui/mojom/diagnostics_service.mojom.h"
#include "chromeos/components/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "chromeos/components/telemetry_extension_ui/telemetry_extension_ui.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"  // nogncheck
#endif

namespace chrome {
namespace internal {

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
void BindUnhandledTapWebContentsObserver(
    content::RenderFrameHost* const host,
    mojo::PendingReceiver<blink::mojom::UnhandledTapNotifier> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(host);
  if (!web_contents)
    return;

  auto* unhandled_tap_notifier_observer =
      contextual_search::UnhandledTapWebContentsObserver::FromWebContents(
          web_contents);
  if (!unhandled_tap_notifier_observer)
    return;

  contextual_search::CreateUnhandledTapNotifierImpl(
      unhandled_tap_notifier_observer->device_scale_factor(),
      unhandled_tap_notifier_observer->unhandled_tap_callback(),
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
void BindContextualSearchObserver(
    content::RenderFrameHost* const host,
    mojo::PendingReceiver<
        contextual_search::mojom::ContextualSearchJsApiService> receiver) {
  // Early return if the RenderFrameHost's delegate is not a WebContents.
  auto* web_contents = content::WebContents::FromRenderFrameHost(host);
  if (!web_contents)
    return;

  auto* contextual_search_observer =
      contextual_search::ContextualSearchObserver::FromWebContents(
          web_contents);
  if (contextual_search_observer) {
    contextual_search::CreateContextualSearchJsApiService(
        contextual_search_observer->api_handler(), std::move(receiver));
  }
}
#endif  // BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)

// Forward image Annotator requests to the profile's AccessibilityLabelsService.
void BindImageAnnotator(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  AccessibilityLabelsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(
          frame_host->GetProcess()->GetBrowserContext()))
      ->BindImageAnnotator(std::move(receiver));
}

void BindDistillabilityService(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<dom_distiller::mojom::DistillabilityService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  dom_distiller::DistillabilityDriver* driver =
      dom_distiller::DistillabilityDriver::FromWebContents(web_contents);
  if (!driver)
    return;
  driver->SetIsSecureCallback(
      base::BindRepeating([](content::WebContents* contents) {
        // SecurityStateTabHelper uses chrome-specific
        // GetVisibleSecurityState to determine if a page is SECURE.
        return SecurityStateTabHelper::FromWebContents(contents)
                   ->GetSecurityLevel() ==
               security_state::SecurityLevel::SECURE;
      }));
  driver->CreateDistillabilityService(std::move(receiver));
}

void BindDistillerJavaScriptService(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<dom_distiller::mojom::DistillerJavaScriptService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  dom_distiller::DomDistillerService* dom_distiller_service =
      dom_distiller::DomDistillerServiceFactory::GetForBrowserContext(
          web_contents->GetBrowserContext());
  auto* distiller_ui_handle = dom_distiller_service->GetDistillerUIHandle();
#if defined(OS_ANDROID)
  static_cast<dom_distiller::android::DistillerUIHandleAndroid*>(
      distiller_ui_handle)
      ->set_render_frame_host(frame_host);
#endif
  auto* distilled_page_prefs = dom_distiller_service->GetDistilledPagePrefs();
  CreateDistillerJavaScriptService(distiller_ui_handle, distilled_page_prefs,
                                   std::move(receiver));
}

void BindPrerenderCanceler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* prerender_contents =
      prerender::ChromePrerenderContentsDelegate::FromWebContents(web_contents);
  if (!prerender_contents)
    return;
  prerender_contents->AddPrerenderCancelerReceiver(std::move(receiver));
}

void BindPrerenderProcessor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::PrerenderProcessor> receiver) {
  prerender::PrerenderProcessorImpl::Create(
      frame_host, std::move(receiver),
      std::make_unique<prerender::ChromePrerenderProcessorImplDelegate>());
}

#if defined(OS_ANDROID)
template <typename Interface>
void ForwardToJavaWebContents(content::RenderFrameHost* frame_host,
                              mojo::PendingReceiver<Interface> receiver) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(frame_host);
  if (contents)
    contents->GetJavaInterfaces()->GetInterface(std::move(receiver));
}

template <typename Interface>
void ForwardToJavaFrame(content::RenderFrameHost* render_frame_host,
                        mojo::PendingReceiver<Interface> receiver) {
  render_frame_host->GetJavaInterfaces()->GetInterface(std::move(receiver));
}
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
void BindMimeHandlerService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::MimeHandlerService>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;
  extensions::MimeHandlerServiceImpl::Create(guest_view->GetStreamWeakPtr(),
                                             std::move(receiver));
}

void BindBeforeUnloadControl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<extensions::mime_handler::BeforeUnloadControl>
        receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* guest_view =
      extensions::MimeHandlerViewGuest::FromWebContents(web_contents);
  if (!guest_view)
    return;
  guest_view->FuseBeforeUnloadControl(std::move(receiver));
}
#endif

void BindNetworkHintsHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<network_hints::mojom::NetworkHintsHandler> receiver) {
  predictors::NetworkHintsHandlerImpl::Create(frame_host, std::move(receiver));
}

#if !defined(OS_ANDROID)
void BindSpeechRecognitionContextHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
  PrefService* profile_prefs = profile->GetPrefs();
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
    SpeechRecognitionServiceFactory::GetForProfile(profile)->Create(
        std::move(receiver));
  }
}

void BindCaptionContextHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<chrome::mojom::CaptionHost> receiver) {
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
  PrefService* profile_prefs = profile->GetPrefs();
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled)) {
    captions::CaptionHostImpl::Create(frame_host, std::move(receiver));
  }
}
#endif

void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));

  map->Add<blink::mojom::AnchorElementMetricsHost>(
      base::BindRepeating(&NavigationPredictor::Create));

  map->Add<blink::mojom::InsecureInputService>(
      base::BindRepeating(&InsecureSensitiveInputDriverFactory::BindDriver));

  map->Add<dom_distiller::mojom::DistillabilityService>(
      base::BindRepeating(&BindDistillabilityService));

  map->Add<dom_distiller::mojom::DistillerJavaScriptService>(
      base::BindRepeating(&BindDistillerJavaScriptService));

  map->Add<prerender::mojom::PrerenderCanceler>(
      base::BindRepeating(&BindPrerenderCanceler));

  map->Add<blink::mojom::PrerenderProcessor>(
      base::BindRepeating(&BindPrerenderProcessor));

  if (performance_manager::PerformanceManager::IsAvailable()) {
    map->Add<performance_manager::mojom::DocumentCoordinationUnit>(
        base::BindRepeating(
            &performance_manager::BindDocumentCoordinationUnit));
  }

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&language::BindContentTranslateDriver));

  map->Add<blink::mojom::CredentialManager>(
      base::BindRepeating(&ChromePasswordManagerClient::BindCredentialManager));

#if defined(OS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::InstalledAppProvider>));
  map->Add<payments::mojom::DigitalGoods>(base::BindRepeating(
      &ForwardToJavaFrame<payments::mojom::DigitalGoods>));
#if defined(BROWSER_MEDIA_CONTROLS_MENU)
  map->Add<blink::mojom::MediaControlsMenuHost>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::MediaControlsMenuHost>));
#endif
  map->Add<chrome::mojom::OfflinePageAutoFetcher>(
      base::BindRepeating(&offline_pages::OfflinePageAutoFetcher::Create));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(base::BindRepeating(
        &ForwardToJavaFrame<payments::mojom::PaymentRequest>));
  }
  map->Add<blink::mojom::ShareService>(base::BindRepeating(
      &ForwardToJavaWebContents<blink::mojom::ShareService>));

#if BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)
  map->Add<contextual_search::mojom::ContextualSearchJsApiService>(
      base::BindRepeating(&BindContextualSearchObserver));

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
  map->Add<blink::mojom::UnhandledTapNotifier>(
      base::BindRepeating(&BindUnhandledTapWebContentsObserver));
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)
#endif  // BUILDFLAG(BUILD_CONTEXTUAL_SEARCH)

#else
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&badging::BadgeManager::BindFrameReceiver));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(
        base::BindRepeating(&payments::CreatePaymentRequest));
  }
  map->Add<payments::mojom::PaymentCredential>(
      base::BindRepeating(&payments::CreatePaymentCredential));
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  map->Add<extensions::mime_handler::MimeHandlerService>(
      base::BindRepeating(&BindMimeHandlerService));
  map->Add<extensions::mime_handler::BeforeUnloadControl>(
      base::BindRepeating(&BindBeforeUnloadControl));
#endif

  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));

#if !defined(OS_ANDROID)
  map->Add<media::mojom::SpeechRecognitionContext>(
      base::BindRepeating(&BindSpeechRecognitionContextHandler));
  map->Add<chrome::mojom::CaptionHost>(
      base::BindRepeating(&BindCaptionContextHandler));
#endif
}

void PopulateChromeWebUIFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  RegisterWebUIControllerInterfaceBinder<::mojom::BluetoothInternalsHandler,
                                         BluetoothInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::InterventionsInternalsPageHandler, InterventionsInternalsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      media::mojom::MediaEngagementScoreDetailsProvider, MediaEngagementUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      media_history::mojom::MediaHistoryStore, MediaHistoryUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::OmniboxPageHandler,
                                         OmniboxUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::SiteEngagementDetailsProvider,
                                         SiteEngagementUI>(map);

  RegisterWebUIControllerInterfaceBinder<::mojom::UsbInternalsPageHandler,
                                         UsbInternalsUI>(map);

#if defined(OS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<
      explore_sites_internals::mojom::PageHandler,
      explore_sites::ExploreSitesInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      snippets_internals::mojom::PageHandlerFactory, SnippetsInternalsUI>(map);
#else
  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  if (base::FeatureList::IsEnabled(features::kNearbySharing)) {
    RegisterWebUIControllerInterfaceBinder<
        nearby_share::mojom::DiscoveryManager,
        nearby_share::NearbyShareDialogUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page::mojom::PageHandlerFactory, NewTabPageUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      promo_browser_command::mojom::CommandHandler, NewTabPageUI>(map);

  RegisterWebUIControllerInterfaceBinder<media_feeds::mojom::MediaFeedsStore,
                                         MediaFeedsUI>(map);

  if (base::FeatureList::IsEnabled(features::kReadLater)) {
    RegisterWebUIControllerInterfaceBinder<
        read_later::mojom::PageHandlerFactory, ReadLaterUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<tab_search::mojom::PageHandlerFactory,
                                         TabSearchUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::web_app_internals::WebAppInternalsPageHandler, InternalsUI>(map);
#endif

#if defined(OS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<
      add_supervision::mojom::AddSupervisionHandler,
      chromeos::AddSupervisionUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      app_management::mojom::PageHandlerFactory,
      chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::settings::mojom::UserActionRecorder,
      chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::settings::mojom::SearchHandler,
      chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::cellular_setup::mojom::CellularSetup,
      chromeos::cellular_setup::CellularSetupDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::crostini_installer::mojom::PageHandlerFactory,
      chromeos::CrostiniInstallerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::crostini_upgrader::mojom::PageHandlerFactory,
      chromeos::CrostiniUpgraderUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::file_manager::mojom::PageHandlerFactory,
      chromeos::file_manager::FileManagerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::machine_learning::mojom::PageHandler,
      chromeos::machine_learning::MachineLearningInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::multidevice_setup::mojom::MultiDeviceSetup, chromeos::OobeUI,
      chromeos::multidevice::ProximityAuthUI,
      chromeos::multidevice_setup::MultiDeviceSetupDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::multidevice_setup::mojom::PrivilegedHostDeviceSetter,
      chromeos::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_config::mojom::CrosNetworkConfig,
      chromeos::InternetConfigDialogUI, chromeos::InternetDetailDialogUI,
      chromeos::NetworkUI, chromeos::OobeUI, chromeos::settings::OSSettingsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::printing::printing_manager::mojom::PrintingMetadataProvider,
      chromeos::printing::printing_manager::PrintManagementUI>(map);

  RegisterWebUIControllerInterfaceBinder<cros::mojom::CameraAppDeviceProvider,
                                         chromeos::CameraAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos_camera::mojom::CameraAppHelper, chromeos::CameraAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<help_app_ui::mojom::PageHandlerFactory,
                                         chromeos::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      media_app_ui::mojom::PageHandlerFactory, chromeos::MediaAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_health::mojom::NetworkHealthService,
      chromeos::NetworkUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines,
      chromeos::NetworkUI>(map);
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS) && !defined(OFFICIAL_BUILD)
  if (base::FeatureList::IsEnabled(chromeos::features::kTelemetryExtension)) {
    RegisterWebUIControllerInterfaceBinder<
        chromeos::health::mojom::DiagnosticsService,
        chromeos::TelemetryExtensionUI>(map);
    RegisterWebUIControllerInterfaceBinder<
        chromeos::health::mojom::ProbeService, chromeos::TelemetryExtensionUI>(
        map);
  }
#endif

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<discards::mojom::DetailsProvider,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::GraphDump,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::SiteDataProvider,
                                         DiscardsUI>(map);
#endif

#if BUILDFLAG(ENABLE_FEED_IN_CHROME)
  RegisterWebUIControllerInterfaceBinder<feed_internals::mojom::PageHandler,
                                         FeedInternalsUI>(map);
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterWebUIControllerInterfaceBinder<::mojom::ResetPasswordHandler,
                                         ResetPasswordUI>(map);
#endif

#if !defined(OS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<media::mojom::KaleidoscopeDataProvider,
                                         KaleidoscopeUI, NewTabPageUI>(map);
#endif  // !defined(OS_ANDROID)

#if defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kNearbySharing)) {
    RegisterWebUIControllerInterfaceBinder<
        nearby_share::mojom::NearbyShareSettings,
        chromeos::settings::OSSettingsUI, nearby_share::NearbyShareDialogUI>(
        map);
  }
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
  if (base::FeatureList::IsEnabled(features::kNearbySharing)) {
    RegisterWebUIControllerInterfaceBinder<
        nearby_share::mojom::NearbyShareSettings,
        nearby_share::NearbyShareDialogUI>(map);
  }
  
  if (base::FeatureList::IsEnabled(features::kNewProfilePicker)) {
    RegisterWebUIControllerInterfaceBinder<
        customize_themes::mojom::CustomizeThemesHandlerFactory,
        ProfilePickerUI>(map);
  }
#endif  // !defined(OS_ANDROID) && !defined(OS_CHROMEOS)
}

}  // namespace internal
}  // namespace chrome
