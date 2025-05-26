// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/keyed_services/chrome_browser_context_keyed_service_factories.h"

#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/blocklist_factory.h"
#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/component_loader_factory.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller_factory.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/error_console/error_console_factory.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_allowlist_factory.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/external_install_manager_factory.h"
#include "chrome/browser/extensions/external_provider_manager_factory.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/shared_module_service_factory.h"
#include "chrome/browser/extensions/updater/extension_updater_factory.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/chrome_app_icon_service_factory.h"
#include "chrome/browser/extensions/extension_error_controller_factory.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/plugin_manager.h"
#include "chrome/browser/extensions/warning_badge_service_factory.h"
#include "ppapi/buildflags/buildflags.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/chromeos/extensions/component_extension_content_settings/component_extension_content_settings_allowlist_factory.h"
#include "chrome/browser/extensions/forced_extensions/assessment_assistant_tracker.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace chrome_extensions {

void EnsureChromeBrowserContextKeyedServiceFactoriesBuilt() {
  TRACE_EVENT("browser",
              "chrome_extensions::"
              "EnsureChromeBrowserContextKeyedServiceFactoriesBuilt");
  extensions::ActivityLog::GetFactoryInstance();
  extensions::BlocklistFactory::GetInstance();
  extensions::ChromeExtensionCookiesFactory::GetInstance();
  extensions::ChromeExtensionSystemFactory::GetInstance();
  extensions::ComponentLoaderFactory::GetInstance();
  extensions::CorruptedExtensionReinstallerFactory::GetInstance();
  extensions::CWSInfoServiceFactory::GetInstance();
  extensions::ErrorConsoleFactory::GetInstance();
  extensions::ExtensionActionDispatcher::GetFactoryInstance();
  extensions::ExtensionAllowlistFactory::GetInstance();
  extensions::ExtensionGarbageCollectorFactory::GetInstance();
  extensions::ExtensionManagementFactory::GetInstance();
  extensions::ExtensionUpdaterFactory::GetInstance();
  extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance();
  extensions::ExternalInstallManagerFactory::GetInstance();
  extensions::ExternalProviderManagerFactory::GetInstance();
  extensions::InstallStageTrackerFactory::GetInstance();
  extensions::InstallTrackerFactory::GetInstance();
  extensions::InstallVerifierFactory::GetInstance();
  extensions::PermissionsUpdater::EnsureAssociatedFactoryBuilt();
  extensions::SharedModuleServiceFactory::GetInstance();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionSyncServiceFactory::GetInstance();
  extensions::AccountExtensionTracker::GetFactory();
  extensions::ChromeAppIconServiceFactory::GetInstance();
  extensions::ExtensionErrorControllerFactory::GetInstance();
  extensions::ExtensionGCMAppHandler::GetFactoryInstance();
  extensions::ManifestV2ExperimentManager::GetFactory();
  extensions::MenuManagerFactory::GetInstance();
#if BUILDFLAG(ENABLE_PLUGINS)
  extensions::PluginManager::GetFactoryInstance();
#endif  // BUILDFLAG(ENABLE_PLUGINS)
  extensions::WarningBadgeServiceFactory::GetInstance();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS)
  extensions::AssessmentAssistantTrackerFactory::GetInstance();
  extensions::ComponentExtensionContentSettingsAllowlistFactory::GetInstance();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

}  // namespace chrome_extensions
