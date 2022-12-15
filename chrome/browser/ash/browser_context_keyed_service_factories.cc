// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_context_keyed_service_factories.h"

#include "chrome/browser/ash/android_sms/android_sms_service_factory.h"
#include "chrome/browser/ash/app_restore/full_restore_service_factory.h"
#include "chrome/browser/ash/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/ash/bluetooth/debug_logs_manager_factory.h"
#include "chrome/browser/ash/borealis/borealis_service_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_service_factory.h"
#include "chrome/browser/ash/cert_provisioning/cert_provisioning_scheduler_user_service.h"
#include "chrome/browser/ash/crosapi/persistent_forced_extension_keep_alive.h"
#include "chrome/browser/ash/crostini/crostini_engagement_metrics_service.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ash/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/ash/extensions/input_method_api.h"
#include "chrome/browser/ash/extensions/media_player_api.h"
#include "chrome/browser/ash/file_manager/volume_manager_factory.h"
#include "chrome/browser/ash/file_system_provider/service_factory.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/ash/lock_screen_apps/lock_screen_apps.h"
#include "chrome/browser/ash/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/ash/login/extensions/login_screen_extensions_content_script_manager_factory.h"
#include "chrome/browser/ash/login/extensions/login_screen_extensions_lifetime_manager_factory.h"
#include "chrome/browser/ash/login/signin_partition_manager.h"
#include "chrome/browser/ash/nearby/nearby_dependencies_provider_factory.h"
#include "chrome/browser/ash/nearby/nearby_process_manager_factory.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/ash/platform_keys/key_permissions/user_private_token_kpm_service_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/ash/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/ash/printing/cups_printers_manager_factory.h"
#include "chrome/browser/ash/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/ash/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/ash/printing/synced_printers_manager_factory.h"
#include "chrome/browser/ash/secure_channel/nearby_connector_factory.h"
#include "chrome/browser/ash/smb_client/smb_service_factory.h"
#include "chrome/browser/ash/tether/tether_service_factory.h"
#include "chrome/browser/ui/ash/calendar/calendar_keyed_service_factory.h"
#include "chrome/browser/ui/ash/global_media_controls/cast_media_notification_producer_keyed_service_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(USE_CUPS)
#include "chrome/browser/ash/printing/cups_proxy_service_manager_factory.h"
#include "chrome/browser/extensions/api/printing/printing_api_handler.h"
#endif

namespace ash {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  android_sms::AndroidSmsServiceFactory::GetInstance();
  CalendarKeyedServiceFactory::GetInstance();
  full_restore::FullRestoreServiceFactory::GetInstance();
  HoldingSpaceKeyedServiceFactory::GetInstance();
  AuthPolicyCredentialsManagerFactory::GetInstance();
  bluetooth::DebugLogsManagerFactory::GetInstance();
  borealis::BorealisServiceFactory::GetInstance();
  bruschetta::BruschettaServiceFactory::GetInstance();
  CastMediaNotificationProducerKeyedServiceFactory::GetInstance();
  cert_provisioning::CertProvisioningSchedulerUserServiceFactory::GetInstance();
  crosapi::PersistentForcedExtensionKeepAliveFactory::GetInstance();
  crostini::CrostiniEngagementMetricsService::Factory::GetInstance();
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
  guest_os::GuestOsRegistryServiceFactory::GetInstance();
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
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  printing::print_management::PrintingManagerFactory::GetInstance();
  PrintJobHistoryServiceFactory::GetInstance();
  secure_channel::NearbyConnectorFactory::GetInstance();
  smb_client::SmbServiceFactory::GetInstance();
  SyncedPrintersManagerFactory::GetInstance();
  tether::TetherServiceFactory::GetInstance();
}

}  // namespace ash
