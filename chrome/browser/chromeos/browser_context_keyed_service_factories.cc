// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/browser_context_keyed_service_factories.h"

#include "chrome/browser/chromeos/account_manager/account_manager_migrator.h"
#include "chrome/browser/chromeos/android_sms/android_sms_service_factory.h"
#include "chrome/browser/chromeos/arc/accessibility/arc_accessibility_helper_bridge.h"
#include "chrome/browser/chromeos/bluetooth/debug_logs_manager_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/extensions/file_manager/event_router_factory.h"
#include "chrome/browser/chromeos/extensions/input_method_api.h"
#include "chrome/browser/chromeos/extensions/login_screen/login_state/session_state_changed_event_dispatcher.h"
#include "chrome/browser/chromeos/extensions/media_player_api.h"
#include "chrome/browser/chromeos/extensions/printing/printing_api_handler.h"
#include "chrome/browser/chromeos/extensions/printing_metrics/print_job_finished_event_dispatcher.h"
#include "chrome/browser/chromeos/login/easy_unlock/easy_unlock_service_factory.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_engagement_metrics_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"
#include "chrome/browser/chromeos/policy/user_network_configuration_updater_factory.h"
#include "chrome/browser/chromeos/printing/cups_print_job_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_printers_manager_factory.h"
#include "chrome/browser/chromeos/printing/cups_proxy_service_manager_factory.h"
#include "chrome/browser/chromeos/printing/history/print_job_history_service_factory.h"
#include "chrome/browser/chromeos/printing/synced_printers_manager_factory.h"
#include "chrome/browser/chromeos/smb_client/smb_service_factory.h"
#include "chrome/browser/chromeos/tether/tether_service_factory.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api.h"
#include "chrome/browser/extensions/api/terminal/terminal_private_api.h"
#include "extensions/browser/api/clipboard/clipboard_api.h"
#include "extensions/browser/api/networking_config/networking_config_service_factory.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_private_api.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "extensions/browser/api/webcam_private/webcam_private_api.h"

namespace chromeos {

void EnsureBrowserContextKeyedServiceFactoriesBuilt() {
  AccountManagerMigratorFactory::GetInstance();
  android_sms::AndroidSmsServiceFactory::GetInstance();
  arc::ArcAccessibilityHelperBridge::CreateFactory();
  bluetooth::DebugLogsManagerFactory::GetInstance();
  crostini::CrostiniRegistryServiceFactory::GetInstance();
  CupsPrintJobManagerFactory::GetInstance();
  CupsPrintersManagerFactory::GetInstance();
#if defined(USE_CUPS)
  CupsProxyServiceManagerFactory::GetInstance();
#endif
  EasyUnlockServiceFactory::GetInstance();

  extensions::ClipboardAPI::GetFactoryInstance();
  extensions::InputMethodAPI::GetFactoryInstance();
  extensions::MediaPlayerAPI::GetFactoryInstance();
  extensions::NetworkingConfigServiceFactory::GetInstance();
  extensions::SessionStateChangedEventDispatcher::GetFactoryInstance();
  extensions::PrintingAPIHandler::GetFactoryInstance();
  extensions::PrintJobFinishedEventDispatcher::GetFactoryInstance();
  extensions::TerminalPrivateAPI::GetFactoryInstance();
  extensions::VerifyTrustAPI::GetFactoryInstance();
  extensions::VirtualKeyboardAPI::GetFactoryInstance();
  extensions::WebcamPrivateAPI::GetFactoryInstance();
  file_manager::EventRouterFactory::GetInstance();
  OwnerSettingsServiceChromeOSFactory::GetInstance();
  plugin_vm::PluginVmEngagementMetricsService::Factory::GetInstance();
  policy::PolicyCertServiceFactory::GetInstance();
  policy::UserCloudPolicyTokenForwarderFactory::GetInstance();
  policy::UserNetworkConfigurationUpdaterFactory::GetInstance();
  PrintJobHistoryServiceFactory::GetInstance();
  smb_client::SmbServiceFactory::GetInstance();
  SyncedPrintersManagerFactory::GetInstance();
  TetherServiceFactory::GetInstance();
  VpnServiceFactory::GetInstance();
}

}  // namespace chromeos
