// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_load_service.h"

#include "apps/app_restore_service.h"
#include "apps/launcher.h"
#include "base/notreached.h"
#include "chrome/browser/apps/platform_apps/app_load_service_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionSystem;

namespace apps {

AppLoadService::PostReloadAction::PostReloadAction()
    : action_type(LAUNCH_FOR_RELOAD),
      command_line(base::CommandLine::NO_PROGRAM) {}

AppLoadService::AppLoadService(content::BrowserContext* context)
    : context_(context) {
  extensions::ExtensionRegistry::Get(context_)->AddObserver(this);

  host_registry_observation_.Observe(
      extensions::ExtensionHostRegistry::Get(context_));
}

AppLoadService::~AppLoadService() = default;

void AppLoadService::Shutdown() {
  extensions::ExtensionRegistry::Get(context_)->RemoveObserver(this);
}

void AppLoadService::RestartApplication(const std::string& extension_id) {
  post_reload_actions_[extension_id].action_type = RESTART;
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(context_)->extension_service();
  DCHECK(service);
  service->ReloadExtension(extension_id);
}

void AppLoadService::RestartApplicationIfRunning(
    const std::string& extension_id) {
  if (apps::AppRestoreService::Get(context_)->IsAppRestorable(extension_id))
    RestartApplication(extension_id);
}

bool AppLoadService::LoadAndLaunch(const base::FilePath& extension_path,
                                   const base::CommandLine& command_line,
                                   const base::FilePath& current_dir) {
  extensions::ExtensionService* extension_service =
      ExtensionSystem::Get(context_)->extension_service();
  std::string extension_id;
  if (!extensions::UnpackedInstaller::Create(extension_service)
           ->LoadFromCommandLine(base::FilePath(extension_path), &extension_id,
                                 true /* only_allow_apps */)) {
    return false;
  }

  // Schedule the app to be launched once loaded.
  PostReloadAction& action = post_reload_actions_[extension_id];
  action.action_type = LAUNCH_FOR_LOAD_AND_LAUNCH;
  action.command_line = command_line;
  action.current_dir = current_dir;
  return true;
}

bool AppLoadService::Load(const base::FilePath& extension_path) {
  extensions::ExtensionService* extension_service =
      ExtensionSystem::Get(context_)->extension_service();
  std::string extension_id;
  return extensions::UnpackedInstaller::Create(extension_service)
      ->LoadFromCommandLine(base::FilePath(extension_path), &extension_id,
                            true /* only_allow_apps */);
}

// static
AppLoadService* AppLoadService::Get(content::BrowserContext* context) {
  return apps::AppLoadServiceFactory::GetForBrowserContext(context);
}

void AppLoadService::OnExtensionHostCompletedFirstLoad(
    content::BrowserContext* browser_context,
    extensions::ExtensionHost* host) {
  const Extension* extension = host->extension();
  // It is possible for an extension to be unloaded before it stops loading.
  if (!extension)
    return;
  auto it = post_reload_actions_.find(extension->id());
  if (it == post_reload_actions_.end())
    return;

  switch (it->second.action_type) {
    case LAUNCH_FOR_RELOAD:
      LaunchPlatformApp(context_, extension,
                        extensions::AppLaunchSource::kSourceReload);
      break;
    case RESTART:
      RestartPlatformApp(context_, extension);
      break;
    case LAUNCH_FOR_LOAD_AND_LAUNCH:
      LaunchPlatformAppWithCommandLine(
          context_, extension, it->second.command_line, it->second.current_dir,
          extensions::AppLaunchSource::kSourceLoadAndLaunch);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  post_reload_actions_.erase(it);
}

void AppLoadService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (!extension->is_platform_app())
    return;

  extensions::ExtensionPrefs* extension_prefs =
      extensions::ExtensionPrefs::Get(browser_context);
  if (WasUnloadedForReload(extension->id(), reason) &&
      extension_prefs->IsActive(extension->id()) &&
      !HasPostReloadAction(extension->id())) {
    post_reload_actions_[extension->id()].action_type = LAUNCH_FOR_RELOAD;
  }
}

bool AppLoadService::WasUnloadedForReload(
    const extensions::ExtensionId& extension_id,
    const extensions::UnloadedExtensionReason reason) {
  if (reason == extensions::UnloadedExtensionReason::DISABLE) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(context_);
    return (prefs->GetDisableReasons(extension_id) &
            extensions::disable_reason::DISABLE_RELOAD) != 0;
  }
  return false;
}

bool AppLoadService::HasPostReloadAction(const std::string& extension_id) {
  return post_reload_actions_.find(extension_id) != post_reload_actions_.end();
}

}  // namespace apps
