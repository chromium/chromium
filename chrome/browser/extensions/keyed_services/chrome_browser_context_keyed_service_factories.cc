// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/keyed_services/chrome_browser_context_keyed_service_factories.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/error_console/error_console_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/pending_extension_manager_factory.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/chrome_app_icon_service_factory.h"
#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/extension_action_dispatcher.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/plugin_manager.h"
#include "chrome/browser/extensions/warning_badge_service_factory.h"
#include "ppapi/buildflags/buildflags.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/extensions/forced_extensions/assessment_assistant_tracker.h"
#endif

namespace chrome_extensions {

void EnsureChromeBrowserContextKeyedServiceFactoriesBuilt() {
  extensions::ErrorConsoleFactory::GetInstance();
  extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance();
  extensions::PendingExtensionManagerFactory::GetInstance();
  extensions::PermissionsUpdater::EnsureAssociatedFactoryBuilt();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  ExtensionSyncServiceFactory::GetInstance();
  extensions::AccountExtensionTracker::GetFactory();
  extensions::ActivityLog::GetFactoryInstance();
  extensions::ChromeAppIconServiceFactory::GetInstance();
  extensions::ChromeExtensionCookiesFactory::GetInstance();
  extensions::CWSInfoServiceFactory::GetInstance();
  extensions::ExtensionActionDispatcher::GetFactoryInstance();
  extensions::ExtensionGarbageCollectorFactory::GetInstance();
  extensions::ExtensionGCMAppHandler::GetFactoryInstance();
  extensions::ExtensionManagementFactory::GetInstance();
  extensions::ChromeExtensionSystemFactory::GetInstance();
  extensions::InstallTrackerFactory::GetInstance();
  extensions::InstallVerifierFactory::GetInstance();
  extensions::ManifestV2ExperimentManager::GetFactory();
  extensions::MenuManagerFactory::GetInstance();
#if BUILDFLAG(ENABLE_PLUGINS)
  extensions::PluginManager::GetFactoryInstance();
#endif
  extensions::WarningBadgeServiceFactory::GetInstance();
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
#if BUILDFLAG(IS_CHROMEOS)
  extensions::AssessmentAssistantTrackerFactory::GetInstance();
#endif
}

}  // namespace chrome_extensions
