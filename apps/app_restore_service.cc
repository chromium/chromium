// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_restore_service_factory.h"
#include "apps/launcher.h"
#include "apps/saved_files_service.h"
#include "build/chromeos_buildflags.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;
using extensions::ExtensionHost;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;

namespace apps {

// static
bool AppRestoreService::ShouldRestoreApps(bool is_browser_restart) {
  bool should_restore_apps = is_browser_restart;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Chromeos always restarts apps, even if it was a regular shutdown.
  should_restore_apps = true;
#endif
  return should_restore_apps;
}

AppRestoreService::AppRestoreService(content::BrowserContext* context)
    : context_(context) {
  StartObservingAppLifetime();
}

void AppRestoreService::HandleStartup(bool should_restore_apps) {
  const extensions::ExtensionSet& extensions =
      ExtensionRegistry::Get(context_)->enabled_extensions();
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(context_);

  for (extensions::ExtensionSet::const_iterator it = extensions.begin();
      it != extensions.end(); ++it) {
    const Extension* extension = it->get();
    if (extension_prefs->IsExtensionRunning(extension->id())) {
      RecordAppStop(extension->id());
      // If we are not restoring apps (e.g., because it is a clean restart), and
      // the app does not have retain permission, explicitly clear the retained
      // entries queue.
      if (should_restore_apps) {
        RestoreApp(it->get());
      } else {
        SavedFilesService::Get(context_)->ClearQueueIfNoRetainPermission(
            extension);
      }
    }
  }
}

bool AppRestoreService::IsAppRestorable(
    const extensions::ExtensionId& extension_id) {
  return ExtensionPrefs::Get(context_)->IsExtensionRunning(extension_id);
}

void AppRestoreService::OnApplicationTerminating() {
  // We want to preserve the state when the app begins terminating, so stop
  // listening to app lifetime events.
  StopObservingAppLifetime();
}

// static
AppRestoreService* AppRestoreService::Get(content::BrowserContext* context) {
  return apps::AppRestoreServiceFactory::GetForBrowserContext(context);
}

void AppRestoreService::OnAppStart(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id) {
  RecordAppStart(extension_id);
}

void AppRestoreService::OnAppActivated(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id) {
  RecordAppActiveState(extension_id, true);
}

void AppRestoreService::OnAppDeactivated(
    content::BrowserContext* context,
    const extensions::ExtensionId& extension_id) {
  RecordAppActiveState(extension_id, false);
}

void AppRestoreService::OnAppStop(content::BrowserContext* context,
                                  const extensions::ExtensionId& extension_id) {
  RecordAppStop(extension_id);
}

void AppRestoreService::Shutdown() {
  StopObservingAppLifetime();
}

void AppRestoreService::RecordAppStart(
    const extensions::ExtensionId& extension_id) {
  ExtensionPrefs::Get(context_)->SetExtensionRunning(extension_id, true);
}

void AppRestoreService::RecordAppStop(
    const extensions::ExtensionId& extension_id) {
  ExtensionPrefs::Get(context_)->SetExtensionRunning(extension_id, false);
}

void AppRestoreService::RecordAppActiveState(
    const extensions::ExtensionId& extension_id,
    bool is_active) {
  ExtensionPrefs* extension_prefs = ExtensionPrefs::Get(context_);

  // If the extension isn't running then we will already have recorded whether
  // it is active or not.
  if (!extension_prefs->IsExtensionRunning(extension_id)) {
    return;
  }

  extension_prefs->SetIsActive(extension_id, is_active);
}

void AppRestoreService::RestoreApp(const Extension* extension) {
  RestartPlatformApp(context_, extension);
}

void AppRestoreService::StartObservingAppLifetime() {
  AppLifetimeMonitor* app_lifetime_monitor =
      AppLifetimeMonitorFactory::GetForBrowserContext(context_);
  DCHECK(app_lifetime_monitor);
  app_lifetime_monitor->AddObserver(this);
}

void AppRestoreService::StopObservingAppLifetime() {
  AppLifetimeMonitor* app_lifetime_monitor =
      AppLifetimeMonitorFactory::GetForBrowserContext(context_);
  // This might be NULL in tests.
  if (app_lifetime_monitor)
    app_lifetime_monitor->RemoveObserver(this);
}

}  // namespace apps
