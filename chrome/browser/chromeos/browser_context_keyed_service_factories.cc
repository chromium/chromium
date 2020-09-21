// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/browser_context_keyed_service_factories.h"

#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/chromeos/authpolicy/authpolicy_credentials_manager.h"
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_finished_event_dispatcher.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_factory.h"
#include "chrome/browser/chromeos/file_system_provider/service_factory.h"
#include "chrome/browser/chromeos/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/chromeos/kerberos/kerberos_credentials_manager_factory.h"
#include "chrome/browser/chromeos/launcher_search_provider/launcher_search_provider_service_factory.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/phonehub/phone_hub_manager_factory.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_service_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/print_management/printing_manager_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/tether/tether_service_factory.h"
#include "chrome/browser/chromeos/web_applications/crosh_loader_factory.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_factory.h"

#if defined(USE_CUPS)
#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"
#include "chrome/browser/chromeos/printing/cups_proxy_service_manager_factory.h"
#endif

namespace chromeos {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  AccountManagerMigratorFactory::GetInstance();
  android_sms::AndroidSmsServiceFactory::GetInstance();
  arc::ArcAccessibilityHelperBridge::CreateFactory();
  AuthPolicyCredentialsManagerFactory::GetInstance();
  bluetooth::DebugLogsManagerFactory::GetInstance();
  CroshLoaderFactory::GetInstance();
#if defined(USE_CUPS)
  CupsProxyServiceManagerFactory::GetInstance();
#endif
  CupsPrintersManagerFactory::GetInstance();
  CupsPrintJobManagerFactory::GetInstance();
  EasyUnlockServiceFactory::GetInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
  extensions::MediaPlayerAPI::GetFactoryInstance();
#if defined(USE_CUPS)
  extensions::PrintingAPIHandler::GetFactoryInstance();
  extensions::PrintJobFinishedEventDispatcher::GetFactoryInstance();
#endif
  extensions::SessionStateChangedEventDispatcher::GetFactoryInstance();
  file_manager::EventRouterFactory::GetInstance();
  file_manager::VolumeManagerFactory::GetInstance();
  file_system_provider::ServiceFactory::GetInstance();
  guest_os::GuestOsRegistryServiceFactory::GetInstance();
  ash::HoldingSpaceKeyedServiceFactory::GetInstance();
  KerberosCredentialsManagerFactory::GetInstance();
  launcher_search_provider::ServiceFactory::GetInstance();
  OwnerSettingsServiceChromeOSFactory::GetInstance();
  phonehub::PhoneHubManagerFactory::GetInstance();
  platform_keys::KeyPermissionsServiceFactory::GetInstance();
  plugin_vm::PluginVmEngagementMetricsService::Factory::GetInstance();
  policy::PolicyCertServiceFactory::GetInstance();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
  printing::print_management::PrintingManagerFactory::GetInstance();
  PrintJobHistoryServiceFactory::GetInstance();
  smb_client::SmbServiceFactory::GetInstance();
  SyncedPrintersManagerFactory::GetInstance();
  TetherServiceFactory::GetInstance();
}

}  // namespace chromeos
