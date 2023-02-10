// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_context_keyed_service_factories.h"

#include "ash/webui/help_app_ui/help_app_manager_factory.h"
#include "chrome/browser/accessibility/service/accessibility_service_router_factory.h"
#include "chrome/browser/apps/app_discovery_service/app_discovery_service_factory.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service_factory.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller_factory.h"
#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/app_list/app_sync_ui_state_factory.h"
#include "chrome/browser/ash/app_list/arc/arc_vpn_provider_manager_factory.h"
#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service_factory.h"
#include "chrome/browser/ash/app_mode/kiosk_app_update_service.h"
#include "chrome/browser/ash/app_restore/app_restore_arc_task_handler_factory.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/apps/apk_web_app_service_factory.h"
#include "chrome/browser/ash/arc/session/arc_service_launcher.h"
#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager_factory.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/child_user_service_factory.h"
#include "chrome/browser/ash/child_accounts/event_based_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/family_user_metrics_service_factory.h"
#include "chrome/browser/ash/child_accounts/screen_time_controller_factory.h"
#include "chrome/browser/ash/concierge_helper_service.h"
#include "chrome/browser/ash/crosapi/keystore_service_factory_ash.h"
#include "chrome/browser/ash/crosapi/persistent_forced_extension_keep_alive.h"
#include "chrome/browser/ash/crostini/ansible/ansible_management_service_factory.h"
#include "chrome/browser/ash/crostini/crostini_engagement_metrics_service.h"
#include "chrome/browser/ash/crostini/crostini_package_service.h"
#include "chrome/browser/ash/crostini/crostini_port_forwarder.h"
#include "chrome/browser/ash/crostini/crostini_upgrader.h"
#include "chrome/browser/ash/crostini/throttle/crostini_throttle.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/extensions/input_method_api.h"
#include "chrome/browser/ash/extensions/media_player_api.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/ash/fileapi/recent_model_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_share_path_factory.h"
#include "chrome/browser/ash/guest_os/public/guest_os_service_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/login/extensions/login_screen_extensions_content_script_manager_factory.h"
#include "chrome/browser/ash/login/extensions/login_screen_extensions_lifetime_manager_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/saml/password_sync_token_verifier_factory.h"
#include "chrome/browser/ash/login/signin/auth_error_observer_factory.h"
#include "chrome/browser/ash/login/signin/oauth2_login_manager_factory.h"
#include "chrome/browser/ash/login/signin/offline_signin_limiter_factory.h"
#include "chrome/browser/ash/login/signin/signin_error_notifier_factory.h"
#include "chrome/browser/ash/login/signin/token_handle_fetcher.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/multidevice_setup/auth_token_validator_factory.h"
#include "chrome/browser/ash/multidevice_setup/multidevice_setup_service_factory.h"
#include "chrome/browser/ash/multidevice_setup/oobe_completion_tracker_factory.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service_factory.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/sync/sync_error_notifier_factory.h"
#include "chrome/browser/ash/tether/tether_service_factory.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_manager_factory.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "chrome/browser/ui/webui/settings/ash/os_settings_manager_factory.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/ash/printing/cups_proxy_service_manager_factory.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif

namespace ash {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  android_sms::AndroidSmsServiceFactory::GetInstance();
  AppSyncUIStateFactory::GetInstance();
  app_list::ArcVpnProviderManagerFactory::GetInstance();
  apps::AppDiscoveryServiceFactory::GetInstance();
  apps::ArcAppsFactory::GetInstance();
  arc::ArcServiceLauncher::EnsureFactoriesBuilt();
  ash::AccountManagerPolicyControllerFactory::GetInstance();
  ash::ApkWebAppServiceFactory::GetInstance();
  ash::ArcKioskAppServiceFactory::GetInstance();
  ash::AuthErrorObserverFactory::GetInstance();
  ash::ChildStatusReportingServiceFactory::GetInstance();
  ash::ChildUserServiceFactory::GetInstance();
  ash::ConciergeHelperServiceFactory::GetInstance();
  ash::EventBasedStatusReportingServiceFactory::GetInstance();
  ash::FamilyUserMetricsServiceFactory::GetInstance();
  ash::InSessionPasswordSyncManagerFactory::GetInstance();
  ash::KioskAppUpdateServiceFactory::GetInstance();
  ash::OAuth2LoginManagerFactory::GetInstance();
  ash::OfflineSigninLimiterFactory::GetInstance();
  ash::PasswordSyncTokenVerifierFactory::GetInstance();
  ash::RecentModelFactory::GetInstance();
  ash::ScreenTimeControllerFactory::GetInstance();
  ash::SigninErrorNotifierFactory::GetInstance();
  ash::SyncErrorNotifierFactory::GetInstance();
  ash::SystemLiveCaptionServiceFactory::GetInstance();
  ash::TokenHandleFetcher::EnsureFactoryBuilt();
  ash::app_restore::AppRestoreArcTaskHandlerFactory::GetInstance();
  ash::help_app::HelpAppManagerFactory::GetInstance();
  ash::multidevice_setup::AuthTokenValidatorFactory::GetInstance();
  ash::multidevice_setup::MultiDeviceSetupServiceFactory::GetInstance();
  ash::multidevice_setup::OobeCompletionTrackerFactory::GetInstance();
  ash::personalization_app::PersonalizationAppManagerFactory::GetInstance();
  ash::quick_start::QuickStartConnectivityServiceFactory::GetInstance();
  ash::quick_unlock::QuickUnlockFactory::GetInstance();
  ash::settings::OsSettingsManagerFactory::GetInstance();
  AuthPolicyCredentialsManagerFactory::GetInstance();
  ax::AccessibilityServiceRouterFactory::EnsureFactoryBuilt();
  bluetooth::DebugLogsManagerFactory::GetInstance();
  borealis::BorealisServiceFactory::GetInstance();
  BrowserProcessPlatformPart::EnsureFactoryBuilt();
  bruschetta::BruschettaServiceFactory::GetInstance();
  CalendarKeyedServiceFactory::GetInstance();
  CastMediaNotificationProducerKeyedServiceFactory::GetInstance();
  cert_provisioning::CertProvisioningSchedulerUserServiceFactory::GetInstance();
  crosapi::KeystoreServiceFactoryAsh::GetInstance();
  crosapi::PersistentForcedExtensionKeepAliveFactory::GetInstance();
  CrosSpeechRecognitionServiceFactory::EnsureFactoryBuilt();
  crostini::AnsibleManagementServiceFactory::GetInstance();
  crostini::CrostiniEngagementMetricsService::Factory::GetInstance();
  crostini::CrostiniPackageService::EnsureFactoryBuilt();
  crostini::CrostiniPortForwarder::EnsureFactoryBuilt();
  crostini::CrostiniThrottle::EnsureFactoryBuilt();
  crostini::CrostiniUpgrader::EnsureFactoryBuilt();
#if BUILDFLAG(USE_CUPS)
  CupsProxyServiceManagerFactory::GetInstance();
#endif
  CupsPrintersManagerFactory::GetInstance();
  CupsPrintJobManagerFactory::GetInstance();
  EasyUnlockServiceFactory::GetInstance();
  eche_app::EcheAppManagerFactory::GetInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
  extensions::MediaPlayerAPI::GetFactoryInstance();
#if BUILDFLAG(USE_CUPS)
  extensions::PrintingAPIHandler::GetFactoryInstance();
#endif
  FileChangeServiceFactory::GetInstance();
  file_manager::EventRouterFactory::GetInstance();
  file_manager::VolumeManagerFactory::GetInstance();
  file_system_provider::ServiceFactory::GetInstance();
  full_restore::FullRestoreServiceFactory::GetInstance();
  guest_os::GuestOsRegistryServiceFactory::GetInstance();
  guest_os::GuestOsServiceFactory::GetInstance();
  guest_os::GuestOsSessionTrackerFactory::GetInstance();
  guest_os::GuestOsSharePathFactory::GetInstance();
  HoldingSpaceKeyedServiceFactory::GetInstance();
  KerberosCredentialsManagerFactory::GetInstance();
  LockScreenAppsFactory::GetInstance();
  LoginScreenExtensionsLifetimeManagerFactory::GetInstance();
  LoginScreenExtensionsContentScriptManagerFactory::GetInstance();
  login::SigninPartitionManager::Factory::GetInstance();
  nearby::NearbyDependenciesProviderFactory::GetInstance();
  nearby::NearbyProcessManagerFactory::GetInstance();
  OwnerSettingsServiceAshFactory::GetInstance();
  phonehub::PhoneHubManagerFactory::GetInstance();
  platform_keys::KeyPermissionsServiceFactory::GetInstance();
  platform_keys::UserPrivateTokenKeyPermissionsManagerServiceFactory::
      GetInstance();
  plugin_vm::PluginVmEngagementMetricsService::Factory::GetInstance();
  plugin_vm::PluginVmInstallerFactory::GetInstance();
  plugin_vm::PluginVmManagerFactory::GetInstance();
  policy::UserCloudPolicyManagerAsh::EnsureFactoryBuilt();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  printing::print_management::PrintingManagerFactory::GetInstance();
  PrintJobHistoryServiceFactory::GetInstance();
  secure_channel::NearbyConnectorFactory::GetInstance();
  smb_client::SmbServiceFactory::GetInstance();
  SyncedPrintersManagerFactory::GetInstance();
  tether::TetherServiceFactory::GetInstance();
  TtsEngineExtensionObserverChromeOS::EnsureFactoryBuilt();
}

}  // namespace ash
