// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_browser_context_keyed_service_factories.h"

#include "build/build_config.h"
#include "chrome/browser/extensions/account_extension_tracker.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/api/bookmarks/bookmarks_api_watcher.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "chrome/browser/extensions/api/messaging/incognito_connectability.h"
#include "chrome/browser/extensions/api/notifications/extension_notification_display_helper_factory.h"
#include "chrome/browser/extensions/chrome_app_icon_service_factory.h"
#include "chrome/browser/extensions/chrome_extension_cookies_factory.h"
#include "chrome/browser/extensions/cws_info_service_factory.h"
#include "chrome/browser/extensions/extension_garbage_collector_factory.h"
#include "chrome/browser/extensions/extension_gcm_app_handler.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_sync_service_factory.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/extensions/manifest_v2_experiment_manager.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/permissions/permissions_updater.h"
#include "chrome/browser/extensions/plugin_manager.h"
#include "chrome/browser/extensions/warning_badge_service_factory.h"
#include "ppapi/buildflags/buildflags.h"

namespace chrome_extensions {

void EnsureChromeBrowserContextKeyedServiceFactoriesBuilt() {
  ExtensionSyncServiceFactory::GetInstance();
  extensions::AccountExtensionTracker::GetFactory();
  extensions::ActivityLog::GetFactoryInstance();
  extensions::BookmarksApiWatcher::EnsureFactoryBuilt();
  extensions::ChromeAppIconServiceFactory::GetInstance();
  extensions::ChromeExtensionCookiesFactory::GetInstance();
  extensions::CWSInfoServiceFactory::GetInstance();
  extensions::ExtensionGarbageCollectorFactory::GetInstance();
  extensions::ExtensionGCMAppHandler::GetFactoryInstance();
  extensions::ExtensionManagementFactory::GetInstance();
  extensions::ExtensionNotificationDisplayHelperFactory::GetInstance();
  extensions::ExtensionSystemFactory::GetInstance();
  extensions::ExtensionWebUIOverrideRegistrar::GetFactoryInstance();
  extensions::image_writer::OperationManager::GetFactoryInstance();
  extensions::IncognitoConnectability::EnsureFactoryBuilt();
  extensions::InstallTrackerFactory::GetInstance();
  extensions::InstallVerifierFactory::GetInstance();
  extensions::ManifestV2ExperimentManager::GetFactory();
  extensions::MenuManagerFactory::GetInstance();
  extensions::PermissionsUpdater::EnsureAssociatedFactoryBuilt();
#if BUILDFLAG(ENABLE_PLUGINS)
  extensions::PluginManager::GetFactoryInstance();
#endif
  extensions::WarningBadgeServiceFactory::GetInstance();
}

}  // namespace chrome_extensions
