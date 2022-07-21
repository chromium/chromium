// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/accessibility/accessibility_labels_service.h"
#include "chrome/browser/accessibility/accessibility_labels_service_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/cart/commerce_hint_service.h"
#include "chrome/browser/dom_distiller/dom_distiller_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/media/history/media_history_store.mojom.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/navigation_predictor/anchor_element_preloader.h"
#include "chrome/browser/navigation_predictor/navigation_predictor.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/predictors/network_hints_handler_impl.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_contents_delegate.h"
#include "chrome/browser/preloading/prefetch/no_state_prefetch/chrome_no_state_prefetch_processor_impl_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/translate/translate_frame_binder.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/draggable_region_host_impl.h"
#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/internals/internals_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/media/media_history_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/services/speech/buildflags/buildflags.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/browser/distiller_javascript_service_impl.h"
#include "components/dom_distiller/content/common/mojom/distillability_service.mojom.h"
#include "components/dom_distiller/content/common/mojom/distiller_javascript_service.mojom.h"
#include "components/dom_distiller/core/dom_distiller_service.h"
#include "components/feed/buildflags.h"
#include "components/feed/feed_feature_list.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/live_caption/caption_util.h"
#include "components/live_caption/pref_names.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_contents.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_processor_impl.h"
#include "components/optimization_guide/optimization_guide_internals/webui/optimization_guide_internals_ui.h"
#include "components/payments/content/payment_credential_factory.h"
#include "components/performance_manager/embedder/binders.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/safe_browsing/buildflags.h"
#include "components/security_state/content/content_utils.h"
#include "components/security_state/core/security_state.h"
#include "components/services/screen_ai/buildflags/buildflags.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "components/translate/content/common/translate.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/image_annotation/public/mojom/image_annotation.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/loader/anchor_element_interaction_host.mojom.h"
#include "third_party/blink/public/mojom/loader/navigation_predictor.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_credential.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/blink/public/mojom/prerender/prerender.mojom.h"
#include "third_party/blink/public/public_buildflags.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "components/services/screen_ai/public/cpp/screen_ai_service_router.h"
#include "components/services/screen_ai/public/cpp/screen_ai_service_router_factory.h"
#endif

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
#include "chrome/browser/android/contextualsearch/unhandled_tap_notifier_impl.h"
#include "chrome/browser/android/contextualsearch/unhandled_tap_web_contents_observer.h"
#include "third_party/blink/public/mojom/unhandled_tap_notifier/unhandled_tap_notifier.mojom.h"
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/dom_distiller/distiller_ui_handle_android.h"
#include "chrome/browser/offline_pages/android/offline_page_auto_fetcher.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals.mojom.h"
#include "chrome/browser/ui/webui/explore_sites_internals/explore_sites_internals_ui.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#include "chrome/common/offline_page_auto_fetcher.mojom.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"
#include "third_party/blink/public/mojom/installedapp/installed_app_provider.mojom.h"
#else
#include "chrome/browser/badging/badge_manager.h"
#include "chrome/browser/cart/chrome_cart.mojom.h"
#include "chrome/browser/new_tab_page/modules/drive/drive.mojom.h"
#include "chrome/browser/new_tab_page/modules/feed/feed.mojom.h"
#include "chrome/browser/new_tab_page/modules/photos/photos.mojom.h"
#include "chrome/browser/new_tab_page/modules/task_module/task_module.mojom.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/payments/payment_request_factory.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/feed/feed.mojom.h"
#include "chrome/browser/ui/webui/feed/feed_ui.h"
#include "chrome/browser/ui/webui/image_editor/image_editor.mojom.h"
#include "chrome/browser/ui/webui/image_editor/image_editor_untrusted_ui.h"
#include "chrome/browser/ui/webui/realbox/realbox.mojom.h"
#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/1125897
#endif
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/internals/user_education/user_education_internals.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/search/ntp_features.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#include "ui/webui/resources/js/metrics_reporter/metrics_reporter.mojom.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#endif

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "ui/webui/resources/cr_components/customize_themes/customize_themes.mojom.h"
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "ash/services/multidevice_setup/multidevice_setup_service.h"
#include "ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/common/mojom/accessibility_features.mojom.h"
#include "ash/webui/connectivity_diagnostics/connectivity_diagnostics_ui.h"
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "ash/webui/eche_app_ui/eche_app_ui.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "ash/webui/file_manager/mojom/file_manager.mojom.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "ash/webui/guest_os_installer/guest_os_installer_ui.h"
#include "ash/webui/guest_os_installer/mojom/guest_os_installer.mojom.h"
#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/help_app_ui/help_app_ui.mojom.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.mojom.h"
#include "ash/webui/multidevice_debug/proximity_auth_ui.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "ash/webui/os_feedback_ui/os_feedback_ui.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/print_management/mojom/printing_manager.mojom.h"
#include "ash/webui/print_management/print_management_ui.h"
#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "ash/webui/scanning/scanning_ui.h"
#include "ash/webui/shimless_rma/shimless_rma.h"
#include "ash/webui/system_extensions_internals_ui/mojom/system_extensions_internals_ui.mojom.h"
#include "ash/webui/system_extensions_internals_ui/system_extensions_internals_ui.h"
#include "chrome/browser/apps/digital_goods/digital_goods_factory_impl.h"
#include "chrome/browser/ash/system_extensions/system_extensions_internals_page_handler.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/chromeos/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/chromeos/audio/audio.mojom.h"
#include "chrome/browser/ui/webui/chromeos/audio/audio_ui.h"
#include "chrome/browser/ui/webui/chromeos/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/browser/ui/webui/chromeos/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_picker.mojom.h"
#include "chrome/browser/ui/webui/chromeos/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/chromeos/internet_config_dialog.h"
#include "chrome/browser/ui/webui/chromeos/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/chromeos/launcher_internals/launcher_internals.mojom.h"
#include "chrome/browser/ui/webui/chromeos/launcher_internals/launcher_internals_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "chrome/browser/ui/webui/chromeos/manage_mirrorsync/manage_mirrorsync_ui.h"
#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/chromeos/network_ui.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/chromeos/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/chromeos/vm/vm.mojom.h"
#include "chrome/browser/ui/webui/chromeos/vm/vm_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/browser/ui/webui/nearby_share/public/mojom/nearby_share_settings.mojom.h"  // nogncheck crbug.com/1125897
#include "chrome/browser/ui/webui/settings/ash/os_apps_page/mojom/app_notification_handler.mojom.h"
#include "chrome/browser/ui/webui/settings/ash/search/search.mojom.h"
#include "chrome/browser/ui/webui/settings/ash/search/user_action_recorder.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_settings_ui.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "chromeos/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"  // nogncheck
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"  // nogncheck
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/apps/digital_goods/digital_goods_factory_stub.h"
#include "chrome/browser/apps/digital_goods/digital_goods_lacros.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
#include "chrome/browser/webshare/share_service_impl.h"
#endif
#include "third_party/blink/public/mojom/webshare/webshare.mojom.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_ui.mojom.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#include "ash/webui/sample_system_web_app_ui/untrusted_sample_system_web_app_ui.h"
#endif

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
#include "chrome/browser/accessibility/live_caption_speech_recognition_host.h"
#include "chrome/browser/accessibility/live_caption_unavailability_notifier.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/mojom/speech_recognition.mojom.h"  // nogncheck
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/speech_recognition.mojom.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
#include "chrome/browser/speech/speech_recognition_service_factory.h"
#include "media/mojo/mojom/speech_recognition_service.mojom.h"
#endif  // BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#endif

#if BUILDFLAG(PLATFORM_CFM)
#include "chrome/browser/ui/webui/chromeos/chromebox_for_meetings/network_settings_dialog.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ash::SystemExtensionsInternalsUI::BindInterface(
    mojo::PendingReceiver<ash::mojom::system_extensions_internals::PageHandler>
        receiver) {
  page_handler_ = std::make_unique<SystemExtensionsInternalsPageHandler>(
      Profile::FromWebUI(web_ui()), std::move(receiver));
}
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
      unhandled_tap_notifier_observer->unhandled_tap_callback(),
      std::move(receiver));
}
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

// Forward image Annotator requests to the profile's AccessibilityLabelsService.
void BindImageAnnotator(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<image_annotation::mojom::Annotator> receiver) {
  AccessibilityLabelsServiceFactory::GetForProfile(
      Profile::FromBrowserContext(
          frame_host->GetProcess()->GetBrowserContext()))
      ->BindImageAnnotator(std::move(receiver));
}

void BindCommerceHintObserver(
    content::RenderFrameHost* const frame_host,
    mojo::PendingReceiver<cart::mojom::CommerceHintObserver> receiver) {
  // This is specifically restricting this to main frames, whether they are the
  // main frame of the tab or a <portal> element, while preventing this from
  // working in subframes and fenced frames.
  if (frame_host->GetParent() || frame_host->IsFencedFrameRoot()) {
    mojo::ReportBadMessage(
        "Unexpected the message from subframe or fenced frame.");
    return;
  }

// On Android, commerce hint observer is enabled for all users with the feature
// enabled since the observer is only used for collecting metrics for now, and
// we want to maximize the user population exposed; on Desktop, ChromeCart is
// not available for non-signin single-profile users and therefore neither does
// commerce hint observer.
#if !BUILDFLAG(IS_ANDROID)
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!identity_manager || !profile_manager)
    return;
  if (!identity_manager->HasPrimaryAccount(signin::ConsentLevel::kSignin) &&
      profile_manager->GetNumberOfProfiles() <= 1)
    return;
#endif
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  if (!browser_context)
    return;
  if (browser_context->IsOffTheRecord())
    return;

  cart::CommerceHintService::CreateForWebContents(web_contents);
  cart::CommerceHintService* service =
      cart::CommerceHintService::FromWebContents(web_contents);
  if (!service)
    return;
  service->BindCommerceHintObserver(frame_host, std::move(receiver));
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
#if BUILDFLAG(IS_ANDROID)
  static_cast<dom_distiller::android::DistillerUIHandleAndroid*>(
      dom_distiller_service->GetDistillerUIHandle())
      ->set_render_frame_host(frame_host);
#endif
  CreateDistillerJavaScriptService(dom_distiller_service->GetWeakPtr(),
                                   std::move(receiver));
}

void BindPrerenderCanceler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<prerender::mojom::PrerenderCanceler> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents)
    return;

  auto* no_state_prefetch_contents =
      prerender::ChromeNoStatePrefetchContentsDelegate::FromWebContents(
          web_contents);
  if (!no_state_prefetch_contents)
    return;
  no_state_prefetch_contents->AddPrerenderCancelerReceiver(std::move(receiver));
}

void BindNoStatePrefetchProcessor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<blink::mojom::NoStatePrefetchProcessor> receiver) {
  prerender::NoStatePrefetchProcessorImpl::Create(
      frame_host, std::move(receiver),
      std::make_unique<
          prerender::ChromeNoStatePrefetchProcessorImplDelegate>());
}

#if BUILDFLAG(IS_ANDROID)
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

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
void BindSpeechRecognitionContextHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver) {
  if (!captions::IsLiveCaptionFeatureSupported()) {
    return;
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // On LaCrOS, forward to Ash.
  auto* service = chromeos::LacrosService::Get();
  if (service && service->IsAvailable<crosapi::mojom::SpeechRecognition>()) {
    service->GetRemote<crosapi::mojom::SpeechRecognition>()
        ->BindSpeechRecognitionContext(std::move(receiver));
  }
#else
  // On other platforms (Ash, desktop), bind via the appropriate factory.
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
#if BUILDFLAG(ENABLE_BROWSER_SPEECH_SERVICE)
  auto* factory = SpeechRecognitionServiceFactory::GetForProfile(profile);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  auto* factory = CrosSpeechRecognitionServiceFactory::GetForProfile(profile);
#else
#error "No speech recognition service factory on this platform."
#endif
  factory->BindSpeechRecognitionContext(std::move(receiver));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void BindSpeechRecognitionClientBrowserInterfaceHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionClientBrowserInterface>
        receiver) {
  if (captions::IsLiveCaptionFeatureSupported()) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // On LaCrOS, forward to Ash.
    auto* service = chromeos::LacrosService::Get();
    if (service && service->IsAvailable<crosapi::mojom::SpeechRecognition>()) {
      service->GetRemote<crosapi::mojom::SpeechRecognition>()
          ->BindSpeechRecognitionClientBrowserInterface(std::move(receiver));
    }
#else
    // On other platforms (Ash, desktop), bind in this process.
    Profile* profile = Profile::FromBrowserContext(
        frame_host->GetProcess()->GetBrowserContext());
    SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(profile)
        ->BindReceiver(std::move(receiver));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
  }
}

void BindSpeechRecognitionRecognizerClientHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::SpeechRecognitionRecognizerClient>
        receiver) {
  Profile* profile = Profile::FromBrowserContext(
      frame_host->GetProcess()->GetBrowserContext());
  PrefService* profile_prefs = profile->GetPrefs();
  if (profile_prefs->GetBoolean(prefs::kLiveCaptionEnabled) &&
      captions::IsLiveCaptionFeatureSupported()) {
    captions::LiveCaptionSpeechRecognitionHost::Create(frame_host,
                                                       std::move(receiver));
  }
}

void BindMediaFoundationRendererNotifierHandler(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<media::mojom::MediaFoundationRendererNotifier>
        receiver) {
  if (captions::IsLiveCaptionFeatureSupported()) {
    captions::LiveCaptionUnavailabilityNotifier::Create(frame_host,
                                                        std::move(receiver));
  }
}
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void BindScreen2xMainContentExtractor(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<screen_ai::mojom::Screen2xMainContentExtractor>
        receiver) {
  ScreenAIServiceRouterFactory::GetForBrowserContext(
      frame_host->GetProcess()->GetBrowserContext())
      ->BindMainContentExtractor(std::move(receiver));
}
#endif

void PopulateChromeFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  map->Add<image_annotation::mojom::Annotator>(
      base::BindRepeating(&BindImageAnnotator));

  // We should not request this mojo interface's binding for the subframes in
  // the renderer.
#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule) &&
#else
  if (base::FeatureList::IsEnabled(commerce::kCommerceHintAndroid) &&
#endif
      !render_frame_host->GetParent() &&
      !render_frame_host->IsFencedFrameRoot()) {
    map->Add<cart::mojom::CommerceHintObserver>(
        base::BindRepeating(&BindCommerceHintObserver));
  }

  map->Add<blink::mojom::AnchorElementMetricsHost>(
      base::BindRepeating(&NavigationPredictor::Create));

  if (base::FeatureList::IsEnabled(
          blink::features::kAnchorElementInteraction)) {
    map->Add<blink::mojom::AnchorElementInteractionHost>(
        base::BindRepeating(&AnchorElementPreloader::Create));
  }

  map->Add<dom_distiller::mojom::DistillabilityService>(
      base::BindRepeating(&BindDistillabilityService));

  map->Add<dom_distiller::mojom::DistillerJavaScriptService>(
      base::BindRepeating(&BindDistillerJavaScriptService));

  map->Add<prerender::mojom::PrerenderCanceler>(
      base::BindRepeating(&BindPrerenderCanceler));

  map->Add<blink::mojom::NoStatePrefetchProcessor>(
      base::BindRepeating(&BindNoStatePrefetchProcessor));

  if (performance_manager::PerformanceManager::IsAvailable()) {
    map->Add<performance_manager::mojom::DocumentCoordinationUnit>(
        base::BindRepeating(
            &performance_manager::BindDocumentCoordinationUnit));
  }

  map->Add<translate::mojom::ContentTranslateDriver>(
      base::BindRepeating(&translate::BindContentTranslateDriver));

  map->Add<blink::mojom::CredentialManager>(
      base::BindRepeating(&ChromePasswordManagerClient::BindCredentialManager));

  map->Add<payments::mojom::PaymentCredential>(
      base::BindRepeating(&payments::CreatePaymentCredential));

#if BUILDFLAG(IS_ANDROID)
  map->Add<blink::mojom::InstalledAppProvider>(base::BindRepeating(
      &ForwardToJavaFrame<blink::mojom::InstalledAppProvider>));
  map->Add<payments::mojom::DigitalGoodsFactory>(base::BindRepeating(
      &ForwardToJavaFrame<payments::mojom::DigitalGoodsFactory>));
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

#if BUILDFLAG(ENABLE_UNHANDLED_TAP)
  map->Add<blink::mojom::UnhandledTapNotifier>(
      base::BindRepeating(&BindUnhandledTapWebContentsObserver));
#endif  // BUILDFLAG(ENABLE_UNHANDLED_TAP)

#else
  map->Add<blink::mojom::BadgeService>(
      base::BindRepeating(&badging::BadgeManager::BindFrameReceiverIfAllowed));
  if (base::FeatureList::IsEnabled(features::kWebPayments)) {
    map->Add<payments::mojom::PaymentRequest>(
        base::BindRepeating(&payments::CreatePaymentRequest));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  map->Add<payments::mojom::DigitalGoodsFactory>(base::BindRepeating(
      &apps::DigitalGoodsFactoryImpl::BindDigitalGoodsFactory));
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (web_app::IsWebAppsCrosapiEnabled()) {
    map->Add<payments::mojom::DigitalGoodsFactory>(
        base::BindRepeating(&apps::DigitalGoodsFactoryLacros::Bind));
  } else {
    map->Add<payments::mojom::DigitalGoodsFactory>(
        base::BindRepeating(&apps::DigitalGoodsFactoryStub::Bind));
  }
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(features::kWebShare)) {
    map->Add<blink::mojom::ShareService>(
        base::BindRepeating(&ShareServiceImpl::Create));
  }
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  map->Add<extensions::mime_handler::MimeHandlerService>(
      base::BindRepeating(&BindMimeHandlerService));
  map->Add<extensions::mime_handler::BeforeUnloadControl>(
      base::BindRepeating(&BindBeforeUnloadControl));
#endif

  map->Add<network_hints::mojom::NetworkHintsHandler>(
      base::BindRepeating(&BindNetworkHintsHandler));

#if BUILDFLAG(ENABLE_SPEECH_SERVICE)
  map->Add<media::mojom::SpeechRecognitionContext>(
      base::BindRepeating(&BindSpeechRecognitionContextHandler));
  map->Add<media::mojom::SpeechRecognitionClientBrowserInterface>(
      base::BindRepeating(&BindSpeechRecognitionClientBrowserInterfaceHandler));
  map->Add<media::mojom::SpeechRecognitionRecognizerClient>(
      base::BindRepeating(&BindSpeechRecognitionRecognizerClientHandler));
  map->Add<media::mojom::MediaFoundationRendererNotifier>(
      base::BindRepeating(&BindMediaFoundationRendererNotifierHandler));
#endif  // BUILDFLAG(ENABLE_SPEECH_SERVICE)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  if (!render_frame_host->GetParent()) {
    map->Add<chrome::mojom::DraggableRegions>(
        base::BindRepeating(&DraggableRegionsHostImpl::CreateIfAllowed));
  }
#endif

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_WIN)
  if (base::FeatureList::IsEnabled(blink::features::kDesktopPWAsSubApps) &&
      render_frame_host->IsInPrimaryMainFrame()) {
    map->Add<blink::mojom::SubAppsService>(
        base::BindRepeating(&web_app::SubAppsServiceImpl::CreateIfAllowed));
  }
#endif

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    map->Add<screen_ai::mojom::Screen2xMainContentExtractor>(
        base::BindRepeating(&BindScreen2xMainContentExtractor));
  }
#endif
}

void PopulateChromeWebUIFrameBinders(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<::mojom::BluetoothInternalsHandler,
                                         BluetoothInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      media::mojom::MediaEngagementScoreDetailsProvider, MediaEngagementUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<browsing_topics::mojom::PageHandler,
                                         BrowsingTopicsInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      media_history::mojom::MediaHistoryStore, MediaHistoryUI>(map);

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterWebUIControllerInterfaceBinder<
      connectors_internals::mojom::PageHandler,
      enterprise_connectors::ConnectorsInternalsUI>(map);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_FUCHSIA)
  RegisterWebUIControllerInterfaceBinder<
      app_management::mojom::PageHandlerFactory, WebAppSettingsUI>(map);
#endif

#if BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<
      explore_sites_internals::mojom::PageHandler,
      explore_sites::ExploreSitesInternalsUI>(map);
#else
  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page_third_party::mojom::PageHandlerFactory,
      NewTabPageThirdPartyUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page::mojom::PageHandlerFactory, NewTabPageUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      most_visited::mojom::MostVisitedPageHandlerFactory, NewTabPageUI,
      NewTabPageThirdPartyUI>(map);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());
  if (history_clusters_service &&
      history_clusters_service->IsJourneysEnabled()) {
    if (base::FeatureList::IsEnabled(features::kSidePanelJourneys) &&
        base::FeatureList::IsEnabled(features::kUnifiedSidePanel)) {
      RegisterWebUIControllerInterfaceBinder<
          history_clusters::mojom::PageHandler, HistoryUI,
          HistoryClustersSidePanelUI>(map);
    } else {
      RegisterWebUIControllerInterfaceBinder<
          history_clusters::mojom::PageHandler, HistoryUI>(map);
    }
  }

  RegisterWebUIControllerInterfaceBinder<
      browser_command::mojom::CommandHandlerFactory, NewTabPageUI, WhatsNewUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<realbox::mojom::PageHandler,
                                         NewTabPageUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      customize_themes::mojom::CustomizeThemesHandlerFactory, NewTabPageUI
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      ,
      ProfileCustomizationUI, ProfilePickerUI, settings::SettingsUI
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
      >(map);

#if !defined(OFFICIAL_BUILD)
  RegisterWebUIControllerInterfaceBinder<foo::mojom::FooHandler, NewTabPageUI>(
      map);
#endif  // !defined(OFFICIAL_BUILD)

  if (base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule)) {
    RegisterWebUIControllerInterfaceBinder<chrome_cart::mojom::CartHandler,
                                           NewTabPageUI>(map);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpDriveModule)) {
    RegisterWebUIControllerInterfaceBinder<drive::mojom::DriveHandler,
                                           NewTabPageUI>(map);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpPhotosModule)) {
    RegisterWebUIControllerInterfaceBinder<photos::mojom::PhotosHandler,
                                           NewTabPageUI>(map);
  }

  if (IsRecipeTasksModuleEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        task_module::mojom::TaskModuleHandler, NewTabPageUI>(map);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFeedModule)) {
    RegisterWebUIControllerInterfaceBinder<ntp::feed::mojom::FeedHandler,
                                           NewTabPageUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      reading_list::mojom::PageHandlerFactory, ReadingListUI>(map);

  if (base::FeatureList::IsEnabled(features::kUnifiedSidePanel)) {
    RegisterWebUIControllerInterfaceBinder<
        side_panel::mojom::BookmarksPageHandlerFactory, BookmarksSidePanelUI>(
        map);
  } else {
    RegisterWebUIControllerInterfaceBinder<
        side_panel::mojom::BookmarksPageHandlerFactory, ReadingListUI>(map);
  }

  if (features::IsReadAnythingEnabled()) {
    if (base::FeatureList::IsEnabled(features::kUnifiedSidePanel)) {
      RegisterWebUIControllerInterfaceBinder<
          read_anything::mojom::PageHandlerFactory, ReadAnythingUI>(map);
    } else {
      RegisterWebUIControllerInterfaceBinder<
          read_anything::mojom::PageHandlerFactory, ReadingListUI>(map);
    }
  }

  RegisterWebUIControllerInterfaceBinder<tab_search::mojom::PageHandlerFactory,
                                         TabSearchUI>(map);
  if (base::FeatureList::IsEnabled(features::kTabSearchUseMetricsReporter)) {
    RegisterWebUIControllerInterfaceBinder<
        metrics_reporter::mojom::PageMetricsHost, TabSearchUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::user_education_internals::UserEducationInternalsPageHandler,
      InternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::app_service_internals::AppServiceInternalsPageHandler,
      AppServiceInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      access_code_cast::mojom::PageHandlerFactory,
      media_router::AccessCodeCastUI>(map);
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  RegisterWebUIControllerInterfaceBinder<
      color_change_listener::mojom::PageHandler, TabStripUI>(map);

  RegisterWebUIControllerInterfaceBinder<tab_strip::mojom::PageHandlerFactory,
                                         TabStripUI>(map);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterWebUIControllerInterfaceBinder<
      ash::file_manager::mojom::PageHandlerFactory,
      ash::file_manager::FileManagerUI>(map);

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

  if (ash::features::IsPersonalizationHubEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::personalization_app::mojom::SearchHandler,
        chromeos::settings::OSSettingsUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      chromeos::settings::app_notification::mojom::AppNotificationsHandler,
      chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::cellular_setup::mojom::CellularSetup,
      chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::auth::mojom::AuthFactorConfig,
                                         chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::auth::mojom::RecoveryFactorEditor,
                                         chromeos::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::cellular_setup::mojom::ESimManager, chromeos::settings::OSSettingsUI,
      chromeos::NetworkUI, chromeos::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::guest_os_installer::mojom::PageHandlerFactory,
      ash::GuestOSInstallerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::crostini_installer::mojom::PageHandlerFactory,
      chromeos::CrostiniInstallerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::crostini_upgrader::mojom::PageHandlerFactory,
      chromeos::CrostiniUpgraderUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::multidevice_setup::mojom::MultiDeviceSetup, chromeos::OobeUI,
      ash::multidevice::ProximityAuthUI,
      chromeos::multidevice_setup::MultiDeviceSetupDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      parent_access_ui::mojom::ParentAccessUIHandler, chromeos::ParentAccessUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::multidevice_setup::mojom::PrivilegedHostDeviceSetter,
      chromeos::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_config::mojom::CrosNetworkConfig,
#if BUILDFLAG(PLATFORM_CFM)
      chromeos::cfm::NetworkSettingsDialogUi,
#endif  // BUILDFLAG(PLATFORM_CFM)
      chromeos::InternetConfigDialogUI, chromeos::InternetDetailDialogUI,
      chromeos::NetworkUI, chromeos::OobeUI, chromeos::settings::OSSettingsUI,
      chromeos::LockScreenNetworkUI, ash::ShimlessRMADialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::printing::printing_manager::mojom::PrintingMetadataProvider,
      ash::printing::printing_manager::PrintManagementUI>(map);

  RegisterWebUIControllerInterfaceBinder<cros::mojom::CameraAppDeviceProvider,
                                         ash::CameraAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::camera_app::mojom::CameraAppHelper, ash::CameraAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::help_app::mojom::PageHandlerFactory, ash::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::local_search_service::mojom::Index, ash::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::help_app::mojom::SearchHandler,
                                         ash::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::SignalingMessageExchanger,
      ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::SystemInfoProvider, ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::eche_app::mojom::UidGenerator,
                                         ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::NotificationGenerator, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::DisplayStreamHandler, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::media_app_ui::mojom::PageHandlerFactory, ash::MediaAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_health::mojom::NetworkHealthService,
      chromeos::NetworkUI, ash::ConnectivityDiagnosticsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines,
      chromeos::NetworkUI, ash::ConnectivityDiagnosticsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::InputDataProvider, ash::DiagnosticsDialogUI>(
      map);

  if (chromeos::features::IsNetworkingInDiagnosticsAppEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::diagnostics::mojom::NetworkHealthProvider,
        ash::DiagnosticsDialogUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::SystemDataProvider, ash::DiagnosticsDialogUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::SystemRoutineController,
      ash::DiagnosticsDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::vm::mojom::VmDiagnosticsProvider, chromeos::VmUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::scanning::mojom::ScanService,
                                         ash::ScanningUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::common::mojom::AccessibilityFeatures, ash::ScanningUI>(map);

  if (base::FeatureList::IsEnabled(ash::features::kOsFeedback)) {
    RegisterWebUIControllerInterfaceBinder<
        ash::os_feedback_ui::mojom::HelpContentProvider, ash::OSFeedbackUI>(
        map);
    RegisterWebUIControllerInterfaceBinder<
        ash::os_feedback_ui::mojom::FeedbackServiceProvider, ash::OSFeedbackUI>(
        map);
  }

  // TODO(crbug.com/1218492): When boot RMA state is available disable this when
  // not in RMA.
  if (ash::features::IsShimlessRMAFlowEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::shimless_rma::mojom::ShimlessRmaService, ash::ShimlessRMADialogUI>(
        map);
  }

  RegisterWebUIControllerInterfaceBinder<
      emoji_picker::mojom::PageHandlerFactory, chromeos::EmojiUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::personalization_app::mojom::WallpaperProvider,
      ash::personalization_app::PersonalizationAppUI>(map);

  if (ash::features::IsPersonalizationHubEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::personalization_app::mojom::AmbientProvider,
        ash::personalization_app::PersonalizationAppUI>(map);

    RegisterWebUIControllerInterfaceBinder<
        ash::personalization_app::mojom::ThemeProvider,
        ash::personalization_app::PersonalizationAppUI>(map);

    RegisterWebUIControllerInterfaceBinder<
        ash::personalization_app::mojom::UserProvider,
        ash::personalization_app::PersonalizationAppUI>(map);

    RegisterWebUIControllerInterfaceBinder<
        ash::personalization_app::mojom::KeyboardBacklightProvider,
        ash::personalization_app::PersonalizationAppUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      launcher_internals::mojom::PageHandlerFactory,
      chromeos::LauncherInternalsUI>(map);

  if (ash::features::IsBluetoothRevampEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        chromeos::bluetooth_config::mojom::CrosBluetoothConfig,
        chromeos::BluetoothPairingDialogUI, chromeos::settings::OSSettingsUI>(
        map);
  }

  RegisterWebUIControllerInterfaceBinder<audio::mojom::PageHandlerFactory,
                                         chromeos::AudioUI>(map);

  if (ash::features::IsFirmwareUpdaterAppEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::firmware_update::mojom::UpdateProvider, ash::FirmwareUpdateAppUI>(
        map);
  }

  if (ash::features::IsDriveFsMirroringEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        chromeos::manage_mirrorsync::mojom::PageHandlerFactory,
        chromeos::ManageMirrorSyncUI>(map);
  }

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<discards::mojom::DetailsProvider,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::GraphDump,
                                         DiscardsUI>(map);

  RegisterWebUIControllerInterfaceBinder<discards::mojom::SiteDataProvider,
                                         DiscardsUI>(map);
#endif

#if BUILDFLAG(ENABLE_FEED_V2) && BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<feed_internals::mojom::PageHandler,
                                         FeedInternalsUI>(map);
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterWebUIControllerInterfaceBinder<::mojom::ResetPasswordHandler,
                                         ResetPasswordUI>(map);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Because Nearby Share is only currently supported for the primary profile,
  // we should only register binders in that scenario. However, we don't want to
  // plumb the profile through to this function, so we 1) ensure that
  // NearbyShareDialogUI will not be created for non-primary profiles, and 2)
  // rely on the BindInterface implementation of OSSettingsUI to ensure that no
  // Nearby Share receivers are bound.
  if (base::FeatureList::IsEnabled(features::kNearbySharing)) {
    RegisterWebUIControllerInterfaceBinder<
        nearby_share::mojom::NearbyShareSettings,
        chromeos::settings::OSSettingsUI, nearby_share::NearbyShareDialogUI>(
        map);
    RegisterWebUIControllerInterfaceBinder<nearby_share::mojom::ContactManager,
                                           chromeos::settings::OSSettingsUI,
                                           nearby_share::NearbyShareDialogUI>(
        map);
    RegisterWebUIControllerInterfaceBinder<
        nearby_share::mojom::DiscoveryManager,
        nearby_share::NearbyShareDialogUI>(map);
    RegisterWebUIControllerInterfaceBinder<nearby_share::mojom::ReceiveManager,
                                           chromeos::settings::OSSettingsUI>(
        map);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void PopulateChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // This function is broken up into sections based on WebUI types.

  // --- Section 1: chrome:// WebUIs:

#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)
  registry.ForWebUI<ash::SampleSystemWebAppUI>()
      .Add<ash::mojom::sample_swa::PageHandlerFactory>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (base::FeatureList::IsEnabled(ash::features::kSystemExtensions)) {
    registry.ForWebUI<ash::SystemExtensionsInternalsUI>()
        .Add<ash::mojom::system_extensions_internals::PageHandler>();
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // --- Section 2: chrome-untrusted:// WebUIs:

#if BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)
  registry.ForWebUI<ash::DemoModeAppUntrustedUI>()
      .Add<ash::mojom::demo_mode::UntrustedPageHandlerFactory>();
  registry.ForWebUI<ash::UntrustedSampleSystemWebAppUI>()
      .Add<ash::mojom::sample_swa::UntrustedPageInterfacesFactory>();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OFFICIAL_BUILD)

#if !BUILDFLAG(IS_ANDROID)
  registry.ForWebUI<image_editor::ImageEditorUntrustedUI>()
      .Add<image_editor::mojom::ImageEditorHandler>();
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && BUILDFLAG(ENABLE_FEED_V2)
  registry.ForWebUI<feed::FeedUI>()
      .Add<feed::mojom::FeedSidePanelHandlerFactory>();
#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace internal
}  // namespace chrome
