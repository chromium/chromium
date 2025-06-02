// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders_webui.h"

#include "base/feature_list.h"
#include "build/android_buildflags.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/media/media_engagement_score_details.mojom.h"
#include "chrome/browser/optimization_guide/optimization_guide_internals_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/history/history_side_panel_coordinator.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals.mojom.h"
#include "chrome/browser/ui/webui/bluetooth_internals/bluetooth_internals_ui.h"
#include "chrome/browser/ui/webui/browsing_topics/browsing_topics_internals_ui.h"
#include "chrome/browser/ui/webui/chrome_urls/chrome_urls_ui.h"
#include "chrome/browser/ui/webui/data_sharing_internals/data_sharing_internals_ui.h"
#include "chrome/browser/ui/webui/engagement/site_engagement_ui.h"
#include "chrome/browser/ui/webui/location_internals/location_internals.mojom.h"
#include "chrome/browser/ui/webui/location_internals/location_internals_ui.h"
#include "chrome/browser/ui/webui/media/media_engagement_ui.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_internals.mojom.h"
#include "chrome/browser/ui/webui/omnibox/omnibox_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/base_dialog_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/privacy_sandbox_internals_ui.h"
#include "chrome/browser/ui/webui/segmentation_internals/segmentation_internals_ui.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals.mojom.h"
#include "chrome/browser/ui/webui/suggest_internals/suggest_internals_ui.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals.mojom.h"
#include "chrome/browser/ui/webui/usb_internals/usb_internals_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "components/browsing_topics/mojom/browsing_topics_internals.mojom.h"
#include "components/commerce/content/browser/commerce_internals_ui.h"
#include "components/commerce/core/internals/mojom/commerce_internals.mojom.h"
#include "components/compose/buildflags.h"
#include "components/data_sharing/public/features.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/history_clusters_internals/webui/history_clusters_internals_ui.h"
#include "components/lens/lens_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/safe_browsing/buildflags.h"
#include "components/services/on_device_translation/buildflags/buildflags.h"
#include "components/site_engagement/core/mojom/site_engagement_details.mojom.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_ui.h"
#include "ui/webui/resources/cr_components/app_management/app_management.mojom.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_DESKTOP_ANDROID)
#include "chrome/browser/ui/webui/discards/discards.mojom.h"
#include "chrome/browser/ui/webui/discards/discards_ui.h"
#include "chrome/browser/ui/webui/discards/site_data.mojom.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/ui/webui/app_settings/web_app_settings_ui.h"
#include "chrome/browser/ui/webui/on_device_translation_internals/on_device_translation_internals_ui.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin.mojom.h"
#include "chrome/browser/ui/webui/signin/history_sync_optin/history_sync_optin_ui.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_ui.h"
#include "components/signin/public/base/signin_switches.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/feed_internals/feed_internals.mojom.h"
#include "chrome/browser/ui/webui/feed_internals/feed_internals_ui.h"
#include "components/commerce/core/commerce_feature_list.h"
#else
#include "chrome/browser/new_tab_page/modules/file_suggestion/drive_suggestion.mojom.h"
#include "chrome/browser/new_tab_page/modules/file_suggestion/microsoft_files.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/authentication/microsoft_auth.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/google_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/calendar/outlook_calendar.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/most_relevant_tab_resumption/most_relevant_tab_resumption.mojom.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"
#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast_ui.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals.mojom.h"
#include "chrome/browser/ui/webui/app_service_internals/app_service_internals_ui.h"
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
#include "chrome/browser/ui/webui/metrics_reporter/metrics_reporter_service.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer.mojom.h"
#include "chrome/browser/ui/webui/new_tab_footer/new_tab_footer_ui.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page.mojom.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "chrome/browser/ui/webui/new_tab_page_third_party/new_tab_page_third_party_ui.h"
#include "chrome/browser/ui/webui/ntp_microsoft_auth/ntp_microsoft_auth_untrusted_ui.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/browser/ui/webui/on_device_internals/on_device_internals_ui.h"
#include "chrome/browser/ui/webui/password_manager/password_manager_ui.h"
#include "chrome/browser/ui/webui/privacy_sandbox/private_state_tokens/private_state_tokens.mojom.h"
#include "chrome/browser/ui/webui/privacy_sandbox/related_website_sets/related_website_sets.mojom.h"
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice.mojom.h"  // nogncheck crbug.com/1125897
#include "chrome/browser/ui/webui/search_engine_choice/search_engine_choice_ui.h"
#include "chrome/browser/ui/webui/settings/settings_ui.h"
#include "chrome/browser/ui/webui/side_panel/bookmarks/bookmarks_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome.mojom.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_chrome_ui.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/wallpaper_search/wallpaper_search.mojom.h"
#include "chrome/browser/ui/webui/side_panel/history/history_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/history_clusters/history_clusters_side_panel_ui.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list.mojom.h"
#include "chrome/browser/ui/webui/side_panel/reading_list/reading_list_ui.h"
#include "chrome/browser/ui/webui/tab_search/tab_search.mojom.h"
#include "chrome/browser/ui/webui/tab_search/tab_search_ui.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals.mojom.h"
#include "chrome/browser/ui/webui/user_education_internals/user_education_internals_ui.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals.mojom.h"
#include "chrome/browser/ui/webui/web_app_internals/web_app_internals_ui.h"
#include "chrome/browser/ui/webui/webui_gallery/webui_gallery_ui.h"
#include "components/commerce/core/mojom/product_specifications.mojom.h"
#include "components/commerce/core/mojom/shopping_service.mojom.h"  // nogncheck crbug.com/1125897
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/search/ntp_features.h"
#include "ui/webui/resources/cr_components/color_change_listener/color_change_listener.mojom.h"
#include "ui/webui/resources/cr_components/customize_color_scheme_mode/customize_color_scheme_mode.mojom.h"
#include "ui/webui/resources/cr_components/help_bubble/custom_help_bubble.mojom.h"
#include "ui/webui/resources/cr_components/history/history.mojom.h"
#include "ui/webui/resources/cr_components/history_clusters/history_clusters.mojom.h"
#include "ui/webui/resources/cr_components/history_embeddings/history_embeddings.mojom.h"
#include "ui/webui/resources/cr_components/most_visited/most_visited.mojom.h"
#include "ui/webui/resources/cr_components/searchbox/searchbox.mojom.h"
#include "ui/webui/resources/cr_components/theme_color_picker/theme_color_picker.mojom.h"
#include "ui/webui/resources/js/browser_command/browser_command.mojom.h"

#if !defined(OFFICIAL_BUILD)
#include "chrome/browser/ui/webui/new_tab_page/foo/foo.mojom.h"  // nogncheck crbug.com/1125897
#endif  // defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/webui/app_home/app_home.mojom.h"
#include "chrome/browser/ui/webui/app_home/app_home_ui.h"
#include "chrome/browser/ui/webui/signin/profile_customization_ui.h"
#include "chrome/browser/ui/webui/signin/profile_picker_ui.h"
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/webui/annotator/mojom/untrusted_annotator.mojom.h"
#include "ash/webui/annotator/untrusted_annotator_ui.h"
#include "ash/webui/boca_ui/boca_ui.h"
#include "ash/webui/boca_ui/mojom/boca.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_helper.mojom.h"
#include "ash/webui/camera_app_ui/camera_app_ui.h"
#include "ash/webui/color_internals/color_internals_ui.h"
#include "ash/webui/color_internals/mojom/color_internals.mojom.h"
#include "ash/webui/common/mojom/accelerator_fetcher.mojom.h"
#include "ash/webui/common/mojom/accessibility_features.mojom.h"
#include "ash/webui/common/mojom/sea_pen.mojom.h"
#include "ash/webui/common/mojom/shortcut_input_provider.mojom.h"
#include "ash/webui/common/mojom/webui_syslog_emitter.mojom.h"
#include "ash/webui/connectivity_diagnostics/connectivity_diagnostics_ui.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/diagnostics_ui/diagnostics_ui.h"
#include "ash/webui/diagnostics_ui/mojom/input_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/system_data_provider.mojom.h"
#include "ash/webui/diagnostics_ui/mojom/system_routine_controller.mojom.h"
#include "ash/webui/eche_app_ui/eche_app_ui.h"
#include "ash/webui/eche_app_ui/mojom/eche_app.mojom.h"
#include "ash/webui/file_manager/file_manager_ui.h"
#include "ash/webui/file_manager/mojom/file_manager.mojom.h"
#include "ash/webui/files_internals/files_internals_ui.h"
#include "ash/webui/files_internals/mojom/files_internals.mojom.h"
#include "ash/webui/firmware_update_ui/firmware_update_app_ui.h"
#include "ash/webui/firmware_update_ui/mojom/firmware_update.mojom.h"
#include "ash/webui/focus_mode/focus_mode_ui.h"
#include "ash/webui/focus_mode/mojom/focus_mode.mojom.h"
#include "ash/webui/graduation/graduation_ui.h"
#include "ash/webui/graduation/mojom/graduation_ui.mojom.h"
#include "ash/webui/growth_internals/growth_internals.mojom.h"
#include "ash/webui/growth_internals/growth_internals_ui.h"
#include "ash/webui/help_app_ui/help_app_ui.h"
#include "ash/webui/help_app_ui/help_app_ui.mojom.h"
#include "ash/webui/help_app_ui/help_app_untrusted_ui.h"
#include "ash/webui/help_app_ui/search/search.mojom.h"
#include "ash/webui/mall/mall_ui.h"
#include "ash/webui/mall/mall_ui.mojom.h"
#include "ash/webui/media_app_ui/media_app_guest_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.h"
#include "ash/webui/media_app_ui/media_app_ui.mojom.h"
#include "ash/webui/media_app_ui/media_app_ui_untrusted.mojom.h"
#include "ash/webui/multidevice_debug/proximity_auth_ui.h"
#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"
#include "ash/webui/os_feedback_ui/os_feedback_ui.h"
#include "ash/webui/os_feedback_ui/os_feedback_untrusted_ui.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/print_management/print_management_ui.h"
#include "ash/webui/print_preview_cros/mojom/destination_provider.mojom.h"
#include "ash/webui/print_preview_cros/print_preview_cros_ui.h"
#include "ash/webui/projector_app/mojom/untrusted_projector.mojom.h"
#include "ash/webui/projector_app/untrusted_projector_ui.h"
#include "ash/webui/recorder_app_ui/mojom/recorder_app.mojom.h"
#include "ash/webui/recorder_app_ui/recorder_app_ui.h"
#include "ash/webui/sanitize_ui/mojom/sanitize_ui.mojom.h"
#include "ash/webui/sanitize_ui/sanitize_ui.h"
#include "ash/webui/scanner_feedback_ui/mojom/scanner_feedback_ui.mojom.h"
#include "ash/webui/scanner_feedback_ui/scanner_feedback_untrusted_ui.h"
#include "ash/webui/scanning/mojom/scanning.mojom.h"
#include "ash/webui/scanning/scanning_ui.h"
#include "ash/webui/shimless_rma/shimless_rma.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ash/webui/shortcut_customization_ui/mojom/shortcut_customization.mojom.h"
#include "ash/webui/shortcut_customization_ui/shortcut_customization_app_ui.h"
#include "ash/webui/vc_background_ui/vc_background_ui.h"
#include "chrome/browser/chromeos/upload_office_to_cloud/upload_office_to_cloud.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision.mojom.h"
#include "chrome/browser/ui/webui/ash/add_supervision/add_supervision_ui.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install.mojom.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_dialog.h"
#include "chrome/browser/ui/webui/ash/app_install/app_install_ui.h"
#include "chrome/browser/ui/webui/ash/audio/audio.mojom.h"
#include "chrome/browser/ui/webui/ash/audio/audio_ui.h"
#include "chrome/browser/ui/webui/ash/bluetooth/bluetooth_pairing_dialog.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer.mojom.h"
#include "chrome/browser/ui/webui/ash/borealis_installer/borealis_installer_ui.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload.mojom.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer.mojom.h"
#include "chrome/browser/ui/webui/ash/crostini_installer/crostini_installer_ui.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader.mojom.h"
#include "chrome/browser/ui/webui/ash/crostini_upgrader/crostini_upgrader_ui.h"
#include "chrome/browser/ui/webui/ash/curtain_ui/remote_maintenance_curtain_ui.h"
#include "chrome/browser/ui/webui/ash/dlp_internals/dlp_internals.mojom.h"
#include "chrome/browser/ui/webui/ash/dlp_internals/dlp_internals_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_picker.mojom.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_search_proxy.h"
#include "chrome/browser/ui/webui/ash/emoji/emoji_ui.h"
#include "chrome/browser/ui/webui/ash/emoji/new_window_proxy.mojom.h"
#include "chrome/browser/ui/webui/ash/emoji/seal.mojom.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_ui.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_ui.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_ui.h"
#include "chrome/browser/ui/webui/ash/internet/internet_config_dialog.h"
#include "chrome/browser/ui/webui/ash/internet/internet_detail_dialog.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals.mojom.h"
#include "chrome/browser/ui/webui/ash/launcher_internals/launcher_internals_ui.h"
#include "chrome/browser/ui/webui/ash/lobster/lobster.mojom.h"
#include "chrome/browser/ui/webui/ash/lock_screen_reauth/lock_screen_network_ui.h"
#include "chrome/browser/ui/webui/ash/login/mojom/screens_factory.mojom.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/mako/mako_ui.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync.mojom.h"
#include "chrome/browser/ui/webui/ash/manage_mirrorsync/manage_mirrorsync_ui.h"
#include "chrome/browser/ui/webui/ash/multidevice_setup/multidevice_setup_dialog.h"
#include "chrome/browser/ui/webui/ash/network_ui/network_ui.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback.mojom.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.h"
#include "chrome/browser/ui/webui/ash/parent_access/parent_access_ui.mojom.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor.mojom.h"
#include "chrome/browser/ui/webui/ash/sensor_info/sensor_info_ui.h"
#include "chrome/browser/ui/webui/ash/set_time/set_time_ui.h"
#include "chrome/browser/ui/webui/ash/settings/os_settings_ui.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_notification_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_parental_controls_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/date_time/mojom/date_time_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/display_settings/display_settings_provider.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/device/input_device_settings/input_device_settings_provider.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/google_drive_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/files/mojom/one_drive_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/people/mojom/graduation_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/mojom/app_permission_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/pages/search/mojom/magic_boost_handler.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/search.mojom.h"
#include "chrome/browser/ui/webui/ash/settings/search/mojom/user_action_recorder.mojom.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration.mojom.h"
#include "chrome/browser/ui/webui/ash/skyvault/local_files_migration_ui.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_credentials_dialog.h"
#include "chrome/browser/ui/webui/ash/smb_shares/smb_share_dialog.h"
#include "chrome/browser/ui/webui/ash/vm/vm.mojom.h"
#include "chrome/browser/ui/webui/ash/vm/vm_ui.h"
#include "chrome/browser/ui/webui/feedback/feedback_ui.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share.mojom.h"
#include "chrome/browser/ui/webui/nearby_share/nearby_share_dialog_ui.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/audio/public/mojom/cros_audio_config.mojom.h"
#include "chromeos/ash/components/emoji/emoji_search.mojom.h"
#include "chromeos/ash/components/kiosk/vision/webui/kiosk_vision_internals.mojom.h"
#include "chromeos/ash/components/kiosk/vision/webui/ui_controller.h"
#include "chromeos/ash/components/local_search_service/public/mojom/index.mojom.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "chromeos/ash/services/bluetooth_config/public/mojom/cros_bluetooth_config.mojom.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom.h"
#include "chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom.h"
#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom.h"
#include "chromeos/ash/services/hotspot_config/public/mojom/cros_hotspot_config.mojom.h"
#include "chromeos/ash/services/ime/public/mojom/input_method_user_data.mojom.h"
#include "chromeos/ash/services/multidevice_setup/multidevice_setup_service.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"  // nogncheck crbug.com/1125897
#include "chromeos/ash/services/orca/public/mojom/orca_service.mojom.h"
#include "chromeos/components/print_management/mojom/printing_manager.mojom.h"  // nogncheck
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/crosapi/mojom/structured_metrics_service.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"  // nogncheck
#include "chromeos/services/network_health/public/mojom/network_diagnostics.mojom.h"  // nogncheck
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"  // nogncheck
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"

#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_ui.mojom.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_untrusted_ui.h"
#include "ash/webui/status_area_internals/mojom/status_area_internals.mojom.h"
#include "ash/webui/status_area_internals/status_area_internals_ui.h"
#endif  // defined(OFFICIAL_BUILD)
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/ui/webui/certificate_manager/certificate_manager_ui.h"
#include "ui/webui/resources/cr_components/certificate_manager/certificate_manager_v2.mojom.h"
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

#if BUILDFLAG(ENABLE_COMPOSE)
#include "chrome/browser/ui/webui/compose/compose_untrusted_ui.h"
#include "chrome/common/compose/compose.mojom.h"
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/webui/signin/batch_upload/batch_upload.mojom.h"
#include "chrome/browser/ui/webui/signin/batch_upload_ui.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation.mojom.h"
#include "chrome/browser/ui/webui/signin/signout_confirmation/signout_confirmation_ui.h"
#include "components/signin/public/base/signin_switches.h"
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/fre/glic_fre_ui.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/host/glic_ui.h"
#endif

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_api.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip.mojom.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
#include "chrome/browser/ui/webui/reset_password/reset_password.mojom.h"
#include "chrome/browser/ui/webui/reset_password/reset_password_ui.h"
#endif  // BUILDFLAG(FULL_SAFE_BROWSING)

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/webui/extensions_zero_state_promo/zero_state_promo_ui.h"
#endif

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

#if !BUILDFLAG(IS_ANDROID)
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
#endif  // !BUILDFLAG(IS_ANDROID)

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

#if BUILDFLAG(ENABLE_GLIC)
  if (glic::GlicEnabling::IsProfileEligible(Profile::FromBrowserContext(
          render_frame_host->GetProcess()->GetBrowserContext()))) {
    // Register binders for all eligible profiles.
    RegisterWebUIControllerInterfaceBinder<glic::mojom::FrePageHandlerFactory,
                                           glic::GlicFreUI>(map);
    // For GlicUI, the WebUI page will check whether Glic is policy-enabled and
    // restrict access if needed. This isn't required for the GlicFreUI.
    RegisterWebUIControllerInterfaceBinder<glic::mojom::PageHandlerFactory,
                                           glic::GlicUI>(map);
  }
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<
      connectors_internals::mojom::PageHandler,
      enterprise_connectors::ConnectorsInternalsUI>(map);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<dlp_internals::mojom::PageHandler,
                                         policy::DlpInternalsUI>(map);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  RegisterWebUIControllerInterfaceBinder<
      app_management::mojom::PageHandlerFactory, WebAppSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      on_device_translation_internals::mojom::PageHandlerFactory,
      OnDeviceTranslationInternalsUI>(map);

  if (base::FeatureList::IsEnabled(switches::kEnableHistorySyncOptin)) {
    RegisterWebUIControllerInterfaceBinder<
        history_sync_optin::mojom::PageHandlerFactory, HistorySyncOptinUI>(map);
  }
#endif

#if !BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<
      search_engine_choice::mojom::PageHandlerFactory, SearchEngineChoiceUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<downloads::mojom::PageHandlerFactory,
                                         DownloadsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page_third_party::mojom::PageHandlerFactory,
      NewTabPageThirdPartyUI>(map);

  if (lens::features::IsLensOverlayEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        lens::mojom::LensSidePanelPageHandlerFactory,
        lens::LensSidePanelUntrustedUI>(map);
    RegisterWebUIControllerInterfaceBinder<lens::mojom::LensPageHandlerFactory,
                                           lens::LensOverlayUntrustedUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      color_change_listener::mojom::PageHandler,
#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
      TabStripUI,
#endif
#if BUILDFLAG(IS_CHROMEOS)
      ash::OobeUI, ash::personalization_app::PersonalizationAppUI,
      ash::vc_background_ui::VcBackgroundUI, ash::settings::OSSettingsUI,
      ash::DiagnosticsDialogUI, ash::FirmwareUpdateAppUI, ash::ScanningUI,
      ash::OSFeedbackUI, ash::ShortcutCustomizationAppUI,
      ash::printing::printing_manager::PrintManagementUI,
      ash::InternetConfigDialogUI, ash::InternetDetailDialogUI, ash::SetTimeUI,
      ash::BluetoothPairingDialogUI, nearby_share::NearbyShareDialogUI,
      ash::cloud_upload::CloudUploadUI, ash::office_fallback::OfficeFallbackUI,
      ash::multidevice_setup::MultiDeviceSetupDialogUI, ash::ParentAccessUI,
      ash::EmojiUI, ash::RemoteMaintenanceCurtainUI,
      ash::app_install::AppInstallDialogUI, ash::SanitizeDialogUI,
      ash::printing::print_preview::PrintPreviewCrosUI,
      ash::extended_updates::ExtendedUpdatesUI, ash::graduation::GraduationUI,
      policy::local_user_files::LocalFilesMigrationUI,
#endif
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
      SignoutConfirmationUI,
#endif
      NewTabPageUI, OmniboxPopupUI, BookmarksSidePanelUI, CustomizeChromeUI,
      UserEducationInternalsUI, ReadingListUI, TabSearchUI, WebuiGalleryUI,
      HistoryClustersSidePanelUI, ShoppingInsightsSidePanelUI,
      media_router::AccessCodeCastUI, commerce::ProductSpecificationsUI,
      NewTabFooterUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      customize_buttons::mojom::CustomizeButtonsHandlerFactory, NewTabPageUI,
      NewTabFooterUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_page::mojom::PageHandlerFactory, NewTabPageUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_tab_footer::mojom::NewTabFooterHandlerFactory, NewTabFooterUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      most_visited::mojom::MostVisitedPageHandlerFactory, NewTabPageUI,
      NewTabPageThirdPartyUI>(map);

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
      customize_color_scheme_mode::mojom::
          CustomizeColorSchemeModeHandlerFactory,
      CustomizeChromeUI, settings::SettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      theme_color_picker::mojom::ThemeColorPickerHandlerFactory,
      CustomizeChromeUI
#if !BUILDFLAG(IS_CHROMEOS)
      ,
      ProfileCustomizationUI, settings::SettingsUI
#endif  // !BUILDFLAG(IS_CHROMEOS)
      >(map);

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  RegisterWebUIControllerInterfaceBinder<
      certificate_manager_v2::mojom::CertificateManagerPageHandlerFactory,
      CertificateManagerUI>(map);
#endif  // BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)

  RegisterWebUIControllerInterfaceBinder<
      help_bubble::mojom::HelpBubbleHandlerFactory, UserEducationInternalsUI,
      settings::SettingsUI, ReadingListUI, NewTabPageUI, CustomizeChromeUI,
      PasswordManagerUI, HistoryUI, lens::LensOverlayUntrustedUI,
      lens::LensSidePanelUntrustedUI
#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
      ,
      ProfilePickerUI
#endif  //! BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
      >(map);

#if !defined(OFFICIAL_BUILD)
  RegisterWebUIControllerInterfaceBinder<foo::mojom::FooHandler, NewTabPageUI>(
      map);
#endif  // !defined(OFFICIAL_BUILD)

  if (IsDriveModuleEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        file_suggestion::mojom::DriveSuggestionHandler, NewTabPageUI>(map);
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

#if BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<
      ash::mojom::HidPreservingBluetoothStateController,
      ash::settings::OSSettingsUI>(map);
#endif  // defined(IS_CHROMEOS)

  RegisterWebUIControllerInterfaceBinder<
      reading_list::mojom::PageHandlerFactory, ReadingListUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      side_panel::mojom::BookmarksPageHandlerFactory, BookmarksSidePanelUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      shopping_service::mojom::ShoppingServiceHandlerFactory,
      BookmarksSidePanelUI, commerce::ProductSpecificationsUI,
      ShoppingInsightsSidePanelUI, HistoryUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      commerce::product_specifications::mojom::
          ProductSpecificationsHandlerFactory,
      commerce::ProductSpecificationsUI, HistoryUI>(map);

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

  RegisterWebUIControllerInterfaceBinder<tab_search::mojom::PageHandlerFactory,
                                         TabSearchUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::user_education_internals::UserEducationInternalsPageHandler,
      UserEducationInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ::mojom::app_service_internals::AppServiceInternalsPageHandler,
      AppServiceInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      access_code_cast::mojom::PageHandlerFactory,
      media_router::AccessCodeCastUI>(map);

  // TODO(crbug.com/398926117): Create a generic mechanism for these interfaces.
  map->Add<metrics_reporter::mojom::PageMetricsHost>(
      base::BindRepeating(&BindMetricsReporterService));
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
  RegisterWebUIControllerInterfaceBinder<tab_strip::mojom::PageHandlerFactory,
                                         TabStripUI>(map);
  RegisterWebUIControllerInterfaceBinder<tabs_api::mojom::TabStripService,
                                         TabStripUI>(map);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<
      ash::file_manager::mojom::PageHandlerFactory,
      ash::file_manager::FileManagerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      add_supervision::mojom::AddSupervisionHandler, ash::AddSupervisionUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      app_management::mojom::PageHandlerFactory, ash::settings::OSSettingsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::mojom::UserActionRecorder, ash::settings::OSSettingsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<ash::settings::mojom::SearchHandler,
                                         ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::personalization_app::mojom::SearchHandler,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::app_notification::mojom::AppNotificationsHandler,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::app_permission::mojom::AppPermissionsHandler,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::app_parental_controls::mojom::AppParentalControlsHandler,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::mojom::InputDeviceSettingsProvider,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::mojom::DisplaySettingsProvider,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::common::mojom::AcceleratorFetcher,
                                         ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::common::mojom::WebUiSyslogEmitter,
                                         ash::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::common::mojom::ShortcutInputProvider, ash::settings::OSSettingsUI,
      ash::ShortcutCustomizationAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::graduation::mojom::GraduationHandler,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::cellular_setup::mojom::CellularSetup, ash::settings::OSSettingsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<chromeos::auth::mojom::InSessionAuth,
                                         ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::auth::mojom::AuthFactorConfig,
                                         ash::settings::OSSettingsUI,
                                         ash::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::auth::mojom::RecoveryFactorEditor,
                                         ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::auth::mojom::PinFactorEditor,
                                         ash::settings::OSSettingsUI,
                                         ash::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::auth::mojom::PasswordFactorEditor,
                                         ash::settings::OSSettingsUI,
                                         ash::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::cellular_setup::mojom::ESimManager, ash::settings::OSSettingsUI,
      ash::NetworkUI, ash::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::borealis_installer::mojom::PageHandlerFactory,
      ash::BorealisInstallerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::crostini_installer::mojom::PageHandlerFactory,
      ash::CrostiniInstallerUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::crostini_upgrader::mojom::PageHandlerFactory,
      ash::CrostiniUpgraderUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::multidevice_setup::mojom::MultiDeviceSetup, ash::OobeUI,
      ash::multidevice::ProximityAuthUI,
      ash::multidevice_setup::MultiDeviceSetupDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      parent_access_ui::mojom::ParentAccessUiHandler, ash::ParentAccessUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::multidevice_setup::mojom::PrivilegedHostDeviceSetter, ash::OobeUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_config::mojom::CrosNetworkConfig,
      ash::InternetConfigDialogUI, ash::InternetDetailDialogUI, ash::NetworkUI,
      ash::OobeUI, ash::settings::OSSettingsUI, ash::LockScreenNetworkUI,
      ash::FloatingWorkspaceUI, ash::ShimlessRMADialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::connectivity::mojom::PasspointService,
      ash::InternetDetailDialogUI, ash::NetworkUI, ash::settings::OSSettingsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::printing::printing_manager::mojom::PrintingMetadataProvider,
      ash::printing::printing_manager::PrintManagementUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::printing::printing_manager::mojom::PrintManagementHandler,
      ash::printing::printing_manager::PrintManagementUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::help_app::mojom::PageHandlerFactory, ash::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::local_search_service::mojom::Index, ash::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::help_app::mojom::SearchHandler,
                                         ash::HelpAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::SignalingMessageExchanger,
      ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::SystemInfoProvider, ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::AccessibilityProvider, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<ash::eche_app::mojom::UidGenerator,
                                         ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::NotificationGenerator, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::DisplayStreamHandler, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::StreamOrientationObserver,
      ash::eche_app::EcheAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::ConnectionStatusObserver, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::eche_app::mojom::KeyboardLayoutHandler, ash::eche_app::EcheAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::media_app_ui::mojom::PageHandlerFactory, ash::MediaAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_health::mojom::NetworkHealthService, ash::NetworkUI,
      ash::ConnectivityDiagnosticsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      chromeos::network_diagnostics::mojom::NetworkDiagnosticsRoutines,
      ash::NetworkUI, ash::ConnectivityDiagnosticsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::InputDataProvider, ash::DiagnosticsDialogUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::NetworkHealthProvider, ash::DiagnosticsDialogUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::SystemDataProvider, ash::DiagnosticsDialogUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::diagnostics::mojom::SystemRoutineController,
      ash::DiagnosticsDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::vm::mojom::VmDiagnosticsProvider,
                                         ash::VmUI>(map);

  RegisterWebUIControllerInterfaceBinder<ash::scanning::mojom::ScanService,
                                         ash::ScanningUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::common::mojom::AccessibilityFeatures, ash::ScanningUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::os_feedback_ui::mojom::HelpContentProvider, ash::OSFeedbackUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      ash::os_feedback_ui::mojom::FeedbackServiceProvider, ash::OSFeedbackUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::shimless_rma::mojom::ShimlessRmaService, ash::ShimlessRMADialogUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::shortcut_customization::mojom::AcceleratorConfigurationProvider,
      ash::ShortcutCustomizationAppUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::shortcut_customization::mojom::SearchHandler,
      ash::ShortcutCustomizationAppUI>(map);

  if (ash::features::IsPrinterPreviewCrosAppEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::printing::print_preview::mojom::DestinationProvider,
        ash::printing::print_preview::PrintPreviewCrosUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      emoji_picker::mojom::PageHandlerFactory, ash::EmojiUI>(map);

  if (base::FeatureList::IsEnabled(
          ash::features::kImeSystemEmojiPickerMojoSearch)) {
    RegisterWebUIControllerInterfaceBinder<emoji_search::mojom::EmojiSearch,
                                           ash::EmojiUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<sensor::mojom::PageHandlerFactory,
                                         ash::SensorInfoUI>(map);
  RegisterWebUIControllerInterfaceBinder<
      enterprise_reporting::mojom::PageHandlerFactory,
      ash::reporting::EnterpriseReportingUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::personalization_app::mojom::WallpaperProvider,
      ash::personalization_app::PersonalizationAppUI>(map);

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

  RegisterWebUIControllerInterfaceBinder<
      ash::personalization_app::mojom::SeaPenProvider,
      ash::personalization_app::PersonalizationAppUI,
      ash::vc_background_ui::VcBackgroundUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      launcher_internals::mojom::PageHandlerFactory, ash::LauncherInternalsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::bluetooth_config::mojom::CrosBluetoothConfig,
      ash::BluetoothPairingDialogUI, ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::audio_config::mojom::CrosAudioConfig, ash::settings::OSSettingsUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::hotspot_config::mojom::CrosHotspotConfig,
      ash::settings::OSSettingsUI>(map);

  if (base::FeatureList::IsEnabled(
          ash::features::kSystemJapanesePhysicalTyping)) {
    RegisterWebUIControllerInterfaceBinder<
        ash::ime::mojom::InputMethodUserDataService,
        ash::settings::OSSettingsUI>(map);
  }

  if (chromeos::features::IsOrcaEnabled() ||
      chromeos::features::IsMahiEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::settings::magic_boost_handler::mojom::PageHandlerFactory,
        ash::settings::OSSettingsUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<audio::mojom::PageHandlerFactory,
                                         ash::AudioUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::kiosk_vision::mojom::PageConnector, ash::kiosk_vision::UIController>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::extended_updates::mojom::PageHandlerFactory,
      ash::extended_updates::ExtendedUpdatesUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::firmware_update::mojom::UpdateProvider, ash::FirmwareUpdateAppUI>(
      map);

  RegisterWebUIControllerInterfaceBinder<
      ash::firmware_update::mojom::SystemUtils, ash::FirmwareUpdateAppUI>(map);

  if (ash::features::IsDriveFsMirroringEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::manage_mirrorsync::mojom::PageHandlerFactory,
        ash::ManageMirrorSyncUI>(map);
  }

  Profile* profile =
      Profile::FromBrowserContext(render_frame_host->GetBrowserContext());
  if (chromeos::IsEligibleAndEnabledUploadOfficeToCloud(profile)) {
    RegisterWebUIControllerInterfaceBinder<
        ash::cloud_upload::mojom::PageHandlerFactory,
        ash::cloud_upload::CloudUploadUI>(map);
    RegisterWebUIControllerInterfaceBinder<
        ash::office_fallback::mojom::PageHandlerFactory,
        ash::office_fallback::OfficeFallbackUI>(map);
  }

  if (ash::cloud_upload::
          IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled(
              profile)) {
    RegisterWebUIControllerInterfaceBinder<
        ash::settings::one_drive::mojom::PageHandlerFactory,
        ash::settings::OSSettingsUI>(map);
  }

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::google_drive::mojom::PageHandlerFactory,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::settings::date_time::mojom::PageHandlerFactory,
      ash::settings::OSSettingsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::screens_factory::mojom::ScreensFactory, ash::OobeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::app_install::mojom::PageHandlerFactory,
      ash::app_install::AppInstallDialogUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      new_window_proxy::mojom::NewWindowProxy, ash::EmojiUI>(map);
  RegisterWebUIControllerInterfaceBinder<seal::mojom::SealService,
                                         ash::EmojiUI>(map);

  if (base::FeatureList::IsEnabled(features::kSkyVault) &&
      base::FeatureList::IsEnabled(features::kSkyVaultV2)) {
    RegisterWebUIControllerInterfaceBinder<
        policy::local_user_files::mojom::PageHandlerFactory,
        policy::local_user_files::LocalFilesMigrationUI>(map);
  }

  if (ash::features::IsGrowthInternalsEnabled()) {
    RegisterWebUIControllerInterfaceBinder<ash::growth::mojom::PageHandler,
                                           ash::GrowthInternalsUI>(map);
  }
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

#if BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<feed_internals::mojom::PageHandler,
                                         FeedInternalsUI>(map);
#endif

#if BUILDFLAG(FULL_SAFE_BROWSING)
  RegisterWebUIControllerInterfaceBinder<::mojom::ResetPasswordHandler,
                                         ResetPasswordUI>(map);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Because Nearby Share is only currently supported for the primary profile,
  // we should only register binders in that scenario. However, we don't want to
  // plumb the profile through to this function, so we 1) ensure that
  // NearbyShareDialogUI will not be created for non-primary profiles, and 2)
  // rely on the BindInterface implementation of OSSettingsUI to ensure that no
  // Nearby Share receivers are bound.
  RegisterWebUIControllerInterfaceBinder<
      nearby_share::mojom::NearbyShareSettings, ash::settings::OSSettingsUI,
      nearby_share::NearbyShareDialogUI>(map);
  RegisterWebUIControllerInterfaceBinder<nearby_share::mojom::ContactManager,
                                         ash::settings::OSSettingsUI,
                                         nearby_share::NearbyShareDialogUI>(
      map);
  RegisterWebUIControllerInterfaceBinder<nearby_share::mojom::DiscoveryManager,
                                         nearby_share::NearbyShareDialogUI>(
      map);
  RegisterWebUIControllerInterfaceBinder<nearby_share::mojom::ReceiveManager,
                                         ash::settings::OSSettingsUI>(map);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<::app_home::mojom::PageHandlerFactory,
                                         webapps::AppHomeUI>(map);
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
  RegisterWebUIControllerInterfaceBinder<::mojom::WebAppInternalsHandler,
                                         WebAppInternalsUI>(map);
#endif

  RegisterWebUIControllerInterfaceBinder<::mojom::LocationInternalsHandler,
                                         LocationInternalsUI>(map);

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          optimization_guide::features::kOptimizationGuideOnDeviceModel)) {
    RegisterWebUIControllerInterfaceBinder<
        on_device_internals::mojom::PageHandlerFactory,
        on_device_internals::OnDeviceInternalsUI>(map);
  }
#endif

  if (base::FeatureList::IsEnabled(
          privacy_sandbox::kPrivacySandboxInternalsDevUI)) {
    RegisterWebUIControllerInterfaceBinder<
        privacy_sandbox_internals::mojom::PageHandler,
        privacy_sandbox_internals::PrivacySandboxInternalsUI>(map);
  }

#if !BUILDFLAG(IS_ANDROID)
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
      privacy_sandbox::dialog::mojom::BaseDialogPageHandlerFactory,
      privacy_sandbox::BaseDialogUI>(map);
#endif

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  RegisterWebUIControllerInterfaceBinder<
      batch_upload::mojom::PageHandlerFactory, BatchUploadUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      signout_confirmation::mojom::PageHandlerFactory, SignoutConfirmationUI>(
      map);
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

#if BUILDFLAG(IS_CHROMEOS)
  RegisterWebUIControllerInterfaceBinder<ash::focus_mode::mojom::TrackProvider,
                                         ash::FocusModeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::sanitize_ui::mojom::SettingsResetter, ash::SanitizeDialogUI>(map);

  if (ash::features::IsGraduationEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::graduation_ui::mojom::GraduationUiHandler,
        ash::graduation::GraduationUI>(map);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_EXTENSIONS)
  RegisterWebUIControllerInterfaceBinder<
      zero_state_promo::mojom::PageHandlerFactory,
      extensions::ZeroStatePromoController>(map);
  RegisterWebUIControllerInterfaceBinder<
      custom_help_bubble::mojom::CustomHelpBubbleHandlerFactory,
      extensions::ZeroStatePromoController>(map);
#endif
}

void PopulateChromeWebUIFrameInterfaceBrokers(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  // This function is broken up into sections based on WebUI types.

  // --- Section 1: chrome:// WebUIs:

#if BUILDFLAG(IS_CHROMEOS) && !defined(OFFICIAL_BUILD)
  registry.ForWebUI<ash::SampleSystemWebAppUI>()
      .Add<ash::mojom::sample_swa::PageHandlerFactory>()
      .Add<color_change_listener::mojom::PageHandler>();

  registry.ForWebUI<ash::StatusAreaInternalsUI>()
      .Add<ash::mojom::status_area_internals::PageHandler>();
#endif  // BUILDFLAG(IS_CHROMEOS) && !defined(OFFICIAL_BUILD)

#if BUILDFLAG(IS_CHROMEOS)
  registry.ForWebUI<ash::RecorderAppUI>()
      .Add<ash::recorder_app::mojom::PageHandler>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<crosapi::mojom::StructuredMetricsService>();

  registry.ForWebUI<ash::CameraAppUI>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<cros::mojom::CameraAppDeviceProvider>()
      .Add<ash::camera_app::mojom::CameraAppHelper>();
  registry.ForWebUI<ash::ColorInternalsUI>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<ash::color_internals::mojom::WallpaperColorsHandler>();
  registry.ForWebUI<ash::FilesInternalsUI>()
      .Add<ash::mojom::files_internals::PageHandler>();
  registry.ForWebUI<ash::file_manager::FileManagerUI>()
      .Add<color_change_listener::mojom::PageHandler>();
  registry.ForWebUI<ash::smb_dialog::SmbShareDialogUI>()
      .Add<color_change_listener::mojom::PageHandler>();
  registry.ForWebUI<ash::smb_dialog::SmbCredentialsDialogUI>()
      .Add<color_change_listener::mojom::PageHandler>();
  registry.ForWebUI<FeedbackUI>()
      .Add<color_change_listener::mojom::PageHandler>();
  registry.ForWebUI<ash::MallUI>().Add<ash::mall::mojom::PageHandler>();
#endif  // BUILDFLAG(IS_CHROMEOS)

  // --- Section 2: chrome-untrusted:// WebUIs:
#if BUILDFLAG(IS_CHROMEOS)
  registry.ForWebUI<ash::boca::BocaUI>()
      .Add<ash::boca::mojom::BocaPageHandlerFactory>()
      .Add<color_change_listener::mojom::PageHandler>();

  if (chromeos::features::IsOrcaEnabled() ||
      ash::features::IsLobsterEnabled()) {
    registry.ForWebUI<ash::MakoUntrustedUI>()
        .Add<ash::orca::mojom::EditorClient>()
        .Add<lobster::mojom::UntrustedLobsterPageHandler>();
  }

  registry.ForWebUI<ash::DemoModeAppUntrustedUI>()
      .Add<ash::mojom::demo_mode::UntrustedPageHandlerFactory>();

  registry.ForWebUI<ash::UntrustedAnnotatorUI>()
      .Add<ash::annotator::mojom::UntrustedAnnotatorPageHandlerFactory>();

  registry.ForWebUI<ash::UntrustedProjectorUI>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<ash::projector::mojom::UntrustedProjectorPageHandlerFactory>();

  registry.ForWebUI<ash::feedback::OsFeedbackUntrustedUI>()
      .Add<color_change_listener::mojom::PageHandler>();

  registry.ForWebUI<ash::MediaAppGuestUI>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<ash::media_app_ui::mojom::UntrustedServiceFactory>();

  registry.ForWebUI<ash::HelpAppUntrustedUI>()
      .Add<color_change_listener::mojom::PageHandler>();

  registry.ForWebUI<ash::ScannerFeedbackUntrustedUI>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<ash::mojom::scanner_feedback_ui::PageHandler>();
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_CHROMEOS) && !defined(OFFICIAL_BUILD)
  registry.ForWebUI<ash::SampleSystemWebAppUntrustedUI>()
      .Add<ash::mojom::sample_swa::UntrustedPageInterfacesFactory>();
#endif  // BUILDFLAG(IS_CHROMEOS) && !defined(OFFICIAL_BUILD)

#if BUILDFLAG(ENABLE_COMPOSE)
  registry.ForWebUI<ComposeUntrustedUI>()
      .Add<color_change_listener::mojom::PageHandler>()
      .Add<compose::mojom::ComposeSessionUntrustedPageHandlerFactory>();
#endif  // BUILDFLAG(ENABLE_COMPOSE)
#if !BUILDFLAG(IS_ANDROID)
  if (lens::features::IsLensOverlayEnabled()) {
    registry.ForWebUI<lens::LensSidePanelUntrustedUI>()
        .Add<lens::mojom::LensSidePanelPageHandlerFactory>()
        .Add<lens::mojom::LensGhostLoaderPageHandlerFactory>()
        .Add<searchbox::mojom::PageHandler>()
        .Add<help_bubble::mojom::HelpBubbleHandlerFactory>()
        .Add<color_change_listener::mojom::PageHandler>();
  }
  if (lens::features::IsLensOverlayEnabled()) {
    registry.ForWebUI<lens::LensOverlayUntrustedUI>()
        .Add<lens::mojom::LensPageHandlerFactory>()
        .Add<lens::mojom::LensGhostLoaderPageHandlerFactory>()
        .Add<color_change_listener::mojom::PageHandler>()
        .Add<help_bubble::mojom::HelpBubbleHandlerFactory>()
        .Add<searchbox::mojom::PageHandler>();
  }
  registry.ForWebUI<ReadAnythingUntrustedUI>()
      .Add<color_change_listener::mojom::PageHandler>();

  if (data_sharing::features::IsDataSharingFunctionalityEnabled()) {
    registry.ForWebUI<DataSharingUI>()
        .Add<data_sharing::mojom::PageHandlerFactory>()
        .Add<color_change_listener::mojom::PageHandler>();
  }

  registry.ForWebUI<NtpMicrosoftAuthUntrustedUI>()
      .Add<new_tab_page::mojom::
               MicrosoftAuthUntrustedDocumentInterfacesFactory>();

#endif  // !BUILDFLAG(IS_ANDROID)
}

}  // namespace chrome::internal
