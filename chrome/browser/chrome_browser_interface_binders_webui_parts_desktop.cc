// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/ui/actor_overlay_ui.h"
#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_internals.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/tab_groups/tab_groups.mojom.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"
#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/browser/ui/webui/actor_internals/actor_internals.mojom.h"
#include "chrome/browser/ui/webui/actor_internals/actor_internals_ui.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"
#include "chrome/browser/ui/webui/autofill_ml_internals/autofill_ml_internals_ui.h"
#include "chrome/browser/ui/webui/color_pipeline_internals/color_pipeline_internals_ui.h"
#include "chrome/browser/ui/webui/commerce/product_specifications_ui.h"
#include "chrome/browser/ui/webui/commerce/shopping_insights_side_panel_ui.h"
#include "chrome/browser/ui/webui/customize_buttons/customize_buttons.mojom.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing.mojom.h"
#include "chrome/browser/ui/webui/data_sharing/data_sharing_ui.h"
#include "chrome/browser/ui/webui/downloads/downloads.mojom.h"
#include "chrome/browser/ui/webui/downloads/downloads_ui.h"
#include "chrome/browser/ui/webui/history/history_ui.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals.mojom.h"
#include "chrome/browser/ui/webui/infobar_internals/infobar_internals_ui.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals.mojom.h"
#include "chrome/browser/ui/webui/legion_internals/legion_internals_ui.h"
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/action_chips/action_chips.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/ntp_promo/ntp_promo.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets.mojom.h"
#include "chrome/browser/ui/webui/reload_button/reload_button.mojom.h"
#include "chrome/browser/ui/webui/reload_button/reload_button_ui.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice.mojom.h"  // nogncheck crbug.com/1125897
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments.mojom.h"
#include "chrome/browser/ui/webui/side_panel/comments/comments_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search.mojom.h"
#include "chrome/browser/ui/webui/side_panel/history/history_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals.mojom.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals.mojom.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/autofill/core/browser/ml_model/logging/autofill_ml_internals.mojom.h"
#include "components/commerce/core/mojom/product_specifications.mojom.h"
#include "components/commerce/core/mojom/shopping_service.mojom.h"  // nogncheck crbug.com/1125897
#include "components/contextual_tasks/public/features.h"
#include "components/data_sharing/public/features.h"
#include "components/guest_contents/common/guest_contents.mojom.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/legion/features.h"
#include "components/lens/lens_features.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/search/ntp_features.h"
#include "components/sync/base/features.h"
#include "components/user_education/common/user_education_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/webui/color_change_listener/color_change_handler.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/custom_help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"  // nogncheck crbug.com/1125897

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_ui.h"
#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"
#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals_ui.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/graduation/graduation_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_ui.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/print_management/print_management_ui.h"
#include "ash/webui/print_preview_cros/print_preview_cros_ui.h"
#include "ash/webui/sanitize_ui/sanitize_ui.h"
#include "ash/webui/scanning/scanning_ui.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "ash/webui/vc_background_ui/vc_background_ui.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/curtain_ui/remote_maintenance_curtain_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_ui.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/1125897
#endif  // defined(OFFICIAL_BUILD)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "ui/webui/resources/js/batch_upload_promo/batch_upload_promo.mojom.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#endif

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

namespace {

void BindMetricsReporterService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<metrics_reporter::mojom::PageMetricsHost> receiver) {
  auto* web_contents = content::WebContents::FromRenderFrameHost(frame_host);
  if (!web_contents) {
    return;
  }

  // MetricsReporterService is only intended for use by WebUI.
  if (!frame_host->GetWebUI()) {
    return;
  }

  MetricsReporterService* service =
      MetricsReporterService::GetFromWebContents(web_contents);
  service->BindReceiver(std::move(receiver));
}

void BindColorChangeListener(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<color_change_listener::mojom::PageHandler>
        pending_receiver) {
  auto* color_change_handler =
      ui::ColorChangeHandler::GetOrCreateForCurrentDocument(frame_host);
  color_change_handler->Bind(std::move(pending_receiver));
}

}  // namespace

void PopulateChromeWebUIFrameBindersPartsDesktop(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<
      actor_internals::mojom::PageHandlerFactory, ActorInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      search_engine_choice::mojom::PageHandlerFactory, SearchEngineChoiceUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  if (lens::features::IsLensOverlayEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        lens::mojom::LensSidePanelPageHandlerFactory,
        lens::LensSidePanelUntrustedUI>(map);
    RegisterWebUIControllerInterfaceBinder<lens::mojom::LensPageHandlerFactory,
                                           lens::LensOverlayUntrustedUI>(map);
  }

  if (features::kGlicActorUiOverlay.Get()) {
    RegisterWebUIControllerInterfaceBinder<
        actor::ui::mojom::ActorOverlayPageHandlerFactory,
        actor::ui::ActorOverlayUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      customize_buttons::mojom::CustomizeButtonsHandlerFactory, NewTabPageUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page::mojom::PageHandlerFactory, NewTabPageUI>(map);

  if (user_education::features::GetNtpBrowserPromoType() !=
      user_education::features::NtpBrowserPromoType::kNone) {
    RegisterWebUIControllerInterfaceBinder<
        ntp_promo::mojom::NtpPromoHandlerFactory, NewTabPageUI>(map);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpNextFeatures)) {
    RegisterWebUIControllerInterfaceBinder<
        action_chips::mojom::ActionChipsHandlerFactory, NewTabPageUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      most_visited::mojom::MostVisitedPageHandlerFactory, NewTabPageUI>(map);

  if (HistorySidePanelCoordinator::IsSupported()) {
    RegisterWebUIControllerInterfaceBinder<history::mojom::PageHandler,
                                           HistorySidePanelUI, HistoryUI>(map);
  } else {
    RegisterWebUIControllerInterfaceBinder<history::mojom::PageHandler,
                                           HistoryUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      infobar_internals::mojom::PageHandlerFactory, InfoBarInternalsUI>(map);

  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext());
  if (history_clusters_service &&
      history_clusters_service->is_journeys_feature_flag_enabled()) {
    if (HistorySidePanelCoordinator::IsSupported()) {
      RegisterWebUIControllerInterfaceBinder<
          history_clusters::mojom::PageHandler, HistoryUI, HistorySidePanelUI>(
          map);
    } else {
      RegisterWebUIControllerInterfaceBinder<
          history_clusters::mojom::PageHandler, HistoryUI,
          HistoryClustersSidePanelUI>(map);
    }
  }
  if (history_embeddings::IsHistoryEmbeddingsFeatureEnabled()) {
    if (history_clusters_service &&
        history_clusters_service->is_journeys_feature_flag_enabled()) {
      if (HistorySidePanelCoordinator::IsSupported()) {
        RegisterWebUIControllerInterfaceBinder<
            history_embeddings::mojom::PageHandler, HistoryUI,
            HistorySidePanelUI>(map);
      } else {
        RegisterWebUIControllerInterfaceBinder<
            history_embeddings::mojom::PageHandler, HistoryUI,
            HistoryClustersSidePanelUI>(map);
      }
    } else {
      if (HistorySidePanelCoordinator::IsSupported()) {
        RegisterWebUIControllerInterfaceBinder<
            history_embeddings::mojom::PageHandler, HistorySidePanelUI,
            HistoryUI>(map);
      } else {
        RegisterWebUIControllerInterfaceBinder<
            history_embeddings::mojom::PageHandler, HistoryUI>(map);
      }
    }
  }

  if (HistorySidePanelCoordinator::IsSupported()) {
    RegisterWebUIControllerInterfaceBinder<
        page_image_service::mojom::PageImageServiceHandler, HistoryUI,
        HistorySidePanelUI, NewTabPageUI, BookmarksSidePanelUI>(map);
  } else {
    RegisterWebUIControllerInterfaceBinder<
        page_image_service::mojom::PageImageServiceHandler, HistoryUI,
        HistoryClustersSidePanelUI, NewTabPageUI, BookmarksSidePanelUI>(map);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  RegisterWebUIControllerInterfaceBinder<whats_new::mojom::PageHandlerFactory,
                                         WhatsNewUI>(map);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  RegisterWebUIControllerInterfaceBinder<
      batch_upload_promo::mojom::PageHandlerFactory, settings::SettingsUI>(map);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

  RegisterWebUIControllerInterfaceBinder<
      browser_command::mojom::CommandHandlerFactory,
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      WhatsNewUI,
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
      NewTabPageUI>(map);

  RegisterWebUIControllerInterfaceBinder<searchbox::mojom::PageHandler,
                                         NewTabPageUI, OmniboxPopupUI>(map);

  RegisterWebUIControllerInterfaceBinder<suggest_internals::mojom::PageHandler,
                                         SuggestInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      password_manager::mojom::PageHandlerFactory, PasswordManagerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandlerFactory,
      CustomizeChromeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      theme_color_picker::mojom::ThemeColorPickerHandlerFactory,
      CustomizeChromeUI
#if !BUILDFLAG(IS_CHROMEOS)
      ,
      ProfileCustomizationUI
#endif  // !BUILDFLAG(IS_CHROMEOS)
      >(map);

  RegisterWebUIControllerInterfaceBinder<
      help_bubble::mojom::HelpBubbleHandlerFactory, UserEducationInternalsUI,
      ReadingListUI, NewTabPageUI, CustomizeChromeUI, PasswordManagerUI,
      HistoryUI, lens::LensOverlayUntrustedUI, lens::LensSidePanelUntrustedUI
#if !BUILDFLAG(IS_CHROMEOS)
      ,
      ProfilePickerUI
#endif  //! BUILDFLAG(IS_CHROMEOS)
      >(map);

#if !defined(OFFICIAL_BUILD)
  RegisterWebUIControllerInterfaceBinder<foo::mojom::FooHandler, NewTabPageUI>(
      map);
#endif  // !defined(OFFICIAL_BUILD)

  if (IsDriveModuleEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        file_suggestion::mojom::DriveSuggestionHandler, NewTabPageUI>(map);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpTabGroupsModule)) {
    RegisterWebUIControllerInterfaceBinder<ntp::tab_groups::mojom::PageHandler,
                                           NewTabPageUI>(map);
  }

  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpMostRelevantTabResumptionModule)) {
    RegisterWebUIControllerInterfaceBinder<
        ntp::most_relevant_tab_resumption::mojom::PageHandler, NewTabPageUI>(
        map);
  }

  if (base::FeatureList::IsEnabled(ntp_features::kNtpCalendarModule)) {
    RegisterWebUIControllerInterfaceBinder<
        ntp::calendar::mojom::GoogleCalendarPageHandler, NewTabPageUI>(map);
  }

  if (IsOutlookCalendarModuleEnabledForProfile(Profile::FromBrowserContext(
          render_frame_host->GetBrowserContext()))) {
    RegisterWebUIControllerInterfaceBinder<
        ntp::calendar::mojom::OutlookCalendarPageHandler, NewTabPageUI>(map);
  }

  if (IsMicrosoftModuleEnabledForProfile(Profile::FromBrowserContext(
          render_frame_host->GetBrowserContext()))) {
    RegisterWebUIControllerInterfaceBinder<
        ntp::authentication::mojom::MicrosoftAuthPageHandler, NewTabPageUI>(
        map);
  }

  if (IsMicrosoftFilesModuleEnabledForProfile(Profile::FromBrowserContext(
          render_frame_host->GetBrowserContext()))) {
    RegisterWebUIControllerInterfaceBinder<
        file_suggestion::mojom::MicrosoftFilesPageHandler, NewTabPageUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      reading_list::mojom::PageHandlerFactory, ReadingListUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      side_panel::mojom::BookmarksPageHandlerFactory, BookmarksSidePanelUI>(
      map);
  RegisterWebUIControllerInterfaceBinder<comments::mojom::PageHandlerFactory,
                                         CommentsSidePanelUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      shopping_service::mojom::ShoppingServiceHandlerFactory,
      BookmarksSidePanelUI, commerce::ProductSpecificationsUI,
      ShoppingInsightsSidePanelUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      commerce::product_specifications::mojom::
          ProductSpecificationsHandlerFactory,
      commerce::ProductSpecificationsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      commerce::price_tracking::mojom::PriceTrackingHandlerFactory,
      ShoppingInsightsSidePanelUI, BookmarksSidePanelUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      commerce::price_insights::mojom::PriceInsightsHandlerFactory,
      ShoppingInsightsSidePanelUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      side_panel::mojom::CustomizeChromePageHandlerFactory, CustomizeChromeUI>(
      map);

  if (base::FeatureList::IsEnabled(
          ntp_features::kCustomizeChromeWallpaperSearch) &&
      base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideModelExecution)) {
    RegisterWebUIControllerInterfaceBinder<
        side_panel::customize_chrome::mojom::WallpaperSearchHandlerFactory,
        CustomizeChromeUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      side_panel::customize_chrome::mojom::CustomizeToolbarHandlerFactory,
      CustomizeChromeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      read_anything::mojom::UntrustedPageHandlerFactory,
      ReadAnythingUntrustedUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::user_education_internals::UserEducationInternalsPageHandler,
      UserEducationInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::app_service_internals::AppServiceInternalsPageHandler,
      AppServiceInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::autofill_ml_internals::mojom::PageHandler, AutofillMlInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      access_code_cast::mojom::PageHandlerFactory,
      media_router::AccessCodeCastUI>(map);

  // TODO(crbug.com/398926117): Create a generic mechanism for these interfaces.
  map->Add<metrics_reporter::mojom::PageMetricsHost>(
      &BindMetricsReporterService);

  map->Add<color_change_listener::mojom::PageHandler>(&BindColorChangeListener);

  RegisterWebUIControllerInterfaceBinder<::mojom::WebAppInternalsHandler,
                                         WebAppInternalsUI>(map);
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideOnDeviceModel)) {
    RegisterWebUIControllerInterfaceBinder<
        on_device_internals::mojom::PageHandlerFactory,
        on_device_internals::OnDeviceInternalsUI>(map);
  }
  if (base::FeatureList::IsEnabled(privacy_sandbox::kRelatedWebsiteSetsDevUI)) {
    RegisterWebUIControllerInterfaceBinder<
        related_website_sets::mojom::RelatedWebsiteSetsPageHandler,
        privacy_sandbox_internals::PrivacySandboxInternalsUI>(map);
  }

  if (base::FeatureList::IsEnabled(privacy_sandbox::kPrivateStateTokensDevUI)) {
    RegisterWebUIControllerInterfaceBinder<
        private_state_tokens::mojom::PrivateStateTokensPageHandler,
        privacy_sandbox_internals::PrivacySandboxInternalsUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      guest_contents::mojom::GuestContentsHost, WebUIBrowserUI>(map);

  const bool is_ntp_composebox_enabled =
      ntp_composebox::IsNtpComposeboxEnabled(Profile::FromBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext()));
  const bool is_omnibox_aim_popup_enabled = omnibox::IsAimPopupFeatureEnabled();
  const bool is_contextual_tasks_enabled =
      base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks);

  if (is_contextual_tasks_enabled) {
    RegisterWebUIControllerInterfaceBinder<
        contextual_tasks::mojom::PageHandlerFactory, ContextualTasksUI>(map);
  }

  // Registering bindings for all WebUIControllers, even if only one of the
  // features is enabled, as it is too cumbersome and not scalable to account
  // for all combinations of feature flags here.
  // TODO(crbug.com/452983498): This should be fixed by following the
  // registry.ForWebUI() pattern used in
  // PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop below which
  // eliminates the need to account for feature flag combinations.
  if (is_ntp_composebox_enabled || is_omnibox_aim_popup_enabled ||
      is_contextual_tasks_enabled) {
    RegisterWebUIControllerInterfaceBinder<
        composebox::mojom::PageHandlerFactory, NewTabPageUI, ContextualTasksUI,
        OmniboxPopupUI>(map);
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  RegisterWebUIControllerInterfaceBinder<
      app_management::mojom::PageHandlerFactory, WebAppSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      on_device_translation_internals::mojom::PageHandlerFactory,
      OnDeviceTranslationInternalsUI>(map);

  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    RegisterWebUIControllerInterfaceBinder<
        history_sync_optin::mojom::PageHandlerFactory, HistorySyncOptinUI>(map);
  }
  RegisterWebUIControllerInterfaceBinder<::app_home::mojom::PageHandlerFactory,
                                         webapps::AppHomeUI>(map);
#endif

  if (base::FeatureList::IsEnabled(legion::kLegion)) {
    RegisterWebUIControllerInterfaceBinder<
        legion_internals::mojom::LegionInternalsPageHandler, LegionInternalsUI>(
        map);
  }

  auto* contextual_tasks_context_service =
      contextual_tasks::ContextualTasksContextServiceFactory::GetForProfile(
          Profile::FromBrowserContext(
              render_frame_host->GetProcess()->GetBrowserContext()));
  if (contextual_tasks_context_service) {
    RegisterWebUIControllerInterfaceBinder<
        contextual_tasks_internals::mojom::
            ContextualTasksInternalsPageHandlerFactory,
        ContextualTasksUI>(map);
  }
}

void PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsDesktop(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // Note: The MetricsReporterService & ColorChangeListener are available to all
  // WebUIs in the registry
  registry
      .AddGlobal<metrics_reporter::mojom::PageMetricsHost>(
          base::BindRepeating(&BindMetricsReporterService))
      .AddGlobal<color_change_listener::mojom::PageHandler>(
          base::BindRepeating(&BindColorChangeListener));

  registry.ForWebUI<TabSearchUI>().Add<tab_search::mojom::PageHandlerFactory>();

  if (base::FeatureList::IsEnabled(ntp_features::kNtpFooter)) {
    registry.ForWebUI<NewTabFooterUI>()
        .Add<customize_buttons::mojom::CustomizeButtonsHandlerFactory>()
        .Add<new_tab_footer::mojom::NewTabFooterHandlerFactory>()
        .Add<help_bubble::mojom::HelpBubbleHandlerFactory>();
  }

  registry.ForWebUI<NewTabPageThirdPartyUI>()
      .Add<most_visited::mojom::MostVisitedPageHandlerFactory>()
      .Add<new_tab_page_third_party::mojom::PageHandlerFactory>();

  registry
      .ForWebUI<settings::SettingsUI>()
#if !BUILDFLAG(IS_CHROMEOS)
      .Add<theme_color_picker::mojom::ThemeColorPickerHandlerFactory>()
#endif  // !BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      .Add<batch_upload_promo::mojom::PageHandlerFactory>()
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
      .Add<customize_color_scheme_mode::mojom::
               CustomizeColorSchemeModeHandlerFactory>()
      .Add<help_bubble::mojom::HelpBubbleHandlerFactory>();

  // TODO(crbug.com/452983498): Migrate all remaining
  // RegisterWebUIControllerInterfaceBinder calls to registry.ForWebUI().Add()
  // calls.
}

void PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsDesktop(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  if (lens::features::IsLensOverlayEnabled()) {
    registry.ForWebUI<lens::LensSidePanelUntrustedUI>()
        .Add<lens::mojom::LensSidePanelPageHandlerFactory>()
        .Add<lens::mojom::LensGhostLoaderPageHandlerFactory>()
        .Add<searchbox::mojom::PageHandler>()
        .Add<help_bubble::mojom::HelpBubbleHandlerFactory>()
        .Add<composebox::mojom::PageHandlerFactory>();
  }
  if (lens::features::IsLensOverlayEnabled()) {
    registry.ForWebUI<lens::LensOverlayUntrustedUI>()
        .Add<lens::mojom::LensPageHandlerFactory>()
        .Add<lens::mojom::LensGhostLoaderPageHandlerFactory>()
        .Add<help_bubble::mojom::HelpBubbleHandlerFactory>()
        .Add<searchbox::mojom::PageHandler>();
  }
  registry.ForWebUI<ReadAnythingUntrustedUI>();

  if (data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    registry.ForWebUI<DataSharingUI>()
        .Add<data_sharing::mojom::PageHandlerFactory>();
  }

  registry.ForWebUI<NtpMicrosoftAuthUntrustedUI>()
      .Add<new_tab_page::mojom::
               MicrosoftAuthUntrustedDocumentInterfacesFactory>();

  if (webui_browser::IsWebUIBrowserEnabled()) {
    registry.ForWebUI<WebUIBrowserUI>()
        .Add<webui_browser::mojom::PageHandlerFactory>()
        .Add<bookmark_bar::mojom::PageHandlerFactory>()
        .Add<extensions_bar::mojom::PageHandlerFactory>()
        .Add<searchbox::mojom::PageHandler>()
        .Add<tabs_api::mojom::TabStripService>()
        .Add<tracked_element::mojom::TrackedElementHandler>();
  }

  if (features::IsWebUIReloadButtonEnabled()) {
    registry.ForWebUI<ReloadButtonUI>()
        .Add<reload_button::mojom::PageHandlerFactory>();
  }
}

}  // namespace chrome::internal
