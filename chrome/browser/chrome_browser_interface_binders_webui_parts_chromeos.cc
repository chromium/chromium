// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_interface_binders_webui_parts.h"

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/hid_preserving_bluetooth_state_controller.mojom.h"
#include "ash/webui/annotator/mojom/untrusted_annotator.mojom.h"
#include "ash/webui/annotator/untrusted_annotator_ui.h"
#include "ash/webui/boca_receiver_app_ui/boca_receiver_untrusted_ui.h"
#include "ash/webui/boca_receiver_app_ui/mojom/boca_receiver.mojom.h"
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
#include "content/public/browser/web_ui_browser_interface_broker_registry.h"
#include "content/public/browser/web_ui_controller_interface_binder.h"
#include "media/capture/video/chromeos/mojom/camera_app.mojom.h"
#include "mojo/public/cpp/bindings/binder_map.h"

#if !defined(OFFICIAL_BUILD)
#include "ash/webui/sample_system_web_app_ui/mojom/sample_system_web_app_ui.mojom.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_ui.h"
#include "ash/webui/sample_system_web_app_ui/sample_system_web_app_untrusted_ui.h"
#include "ash/webui/status_area_internals/mojom/status_area_internals.mojom.h"
#include "ash/webui/status_area_internals/status_area_internals_ui.h"
#endif  // defined(OFFICIAL_BUILD)

namespace chrome::internal {

using content::RegisterWebUIControllerInterfaceBinder;

void PopulateChromeWebUIFrameBindersPartsCros(
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map,
    content::RenderFrameHost* render_frame_host) {
  RegisterWebUIControllerInterfaceBinder<dlp_internals::mojom::PageHandler,
                                         policy::DlpInternalsUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::mojom::HidPreservingBluetoothStateController,
      ash::settings::OSSettingsUI>(map);

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

  RegisterWebUIControllerInterfaceBinder<
      ash::ime::mojom::InputMethodUserDataService, ash::settings::OSSettingsUI>(
      map);

  if (chromeos::features::IsOrcaEnabled() ||
      chromeos::features::IsMahiEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::settings::magic_boost_handler::mojom::PageHandlerFactory,
        ash::settings::OSSettingsUI>(map);
  }

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

  RegisterWebUIControllerInterfaceBinder<ash::focus_mode::mojom::TrackProvider,
                                         ash::FocusModeUI>(map);

  RegisterWebUIControllerInterfaceBinder<
      ash::sanitize_ui::mojom::SettingsResetter, ash::SanitizeDialogUI>(map);

  if (ash::features::IsGraduationEnabled()) {
    RegisterWebUIControllerInterfaceBinder<
        ash::graduation_ui::mojom::GraduationUiHandler,
        ash::graduation::GraduationUI>(map);
  }
}

void PopulateChromeWebUIFrameInterfaceBrokersTrustedPartsCros(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  registry.ForWebUI<ash::RecorderAppUI>()
      .Add<ash::recorder_app::mojom::PageHandler>()
      .Add<crosapi::mojom::StructuredMetricsService>();

  registry.ForWebUI<ash::CameraAppUI>()
      .Add<cros::mojom::CameraAppDeviceProvider>()
      .Add<ash::camera_app::mojom::CameraAppHelper>();
  registry.ForWebUI<ash::ColorInternalsUI>()
      .Add<ash::color_internals::mojom::WallpaperColorsHandler>();
  registry.ForWebUI<ash::FilesInternalsUI>()
      .Add<ash::mojom::files_internals::PageHandler>();
  registry.ForWebUI<ash::file_manager::FileManagerUI>();
  registry.ForWebUI<ash::smb_dialog::SmbShareDialogUI>();
  registry.ForWebUI<ash::smb_dialog::SmbCredentialsDialogUI>();
  registry.ForWebUI<FeedbackUI>();
  registry.ForWebUI<ash::MallUI>().Add<ash::mall::mojom::PageHandler>();

#if !defined(OFFICIAL_BUILD)
  registry.ForWebUI<ash::SampleSystemWebAppUI>()
      .Add<ash::mojom::sample_swa::PageHandlerFactory>();

  registry.ForWebUI<ash::StatusAreaInternalsUI>()
      .Add<ash::mojom::status_area_internals::PageHandler>();
#endif  // !defined(OFFICIAL_BUILD)
}

void PopulateChromeWebUIFrameInterfaceBrokersUntrustedPartsCros(
    content::WebUIBrowserInterfaceBrokerRegistry& registry) {
  registry.ForWebUI<ash::boca::BocaUI>()
      .Add<ash::boca::mojom::BocaPageHandlerFactory>();

  registry.ForWebUI<ash::BocaReceiverUntrustedUI>()
      .Add<ash::boca_receiver::mojom::UntrustedPageHandlerFactory>();

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
      .Add<ash::projector::mojom::UntrustedProjectorPageHandlerFactory>();

  registry.ForWebUI<ash::feedback::OsFeedbackUntrustedUI>();

  registry.ForWebUI<ash::MediaAppGuestUI>()
      .Add<ash::media_app_ui::mojom::UntrustedServiceFactory>();

  registry.ForWebUI<ash::HelpAppUntrustedUI>();

  registry.ForWebUI<ash::ScannerFeedbackUntrustedUI>()
      .Add<ash::mojom::scanner_feedback_ui::PageHandler>();

#if !defined(OFFICIAL_BUILD)
  registry.ForWebUI<ash::SampleSystemWebAppUntrustedUI>()
      .Add<ash::mojom::sample_swa::UntrustedPageInterfacesFactory>();
#endif  // !defined(OFFICIAL_BUILD)
}

}  // namespace chrome::internal
