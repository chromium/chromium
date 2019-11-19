// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/demo_mode/demo_resources.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/constants/chromeos_paths.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/image_loader_client.h"

namespace chromeos {
namespace {

// Path relative to the path at which demo resources are loaded that
// contains image with demo Android apps.
constexpr base::FilePath::CharType kDemoAppsPath[] =
    FILE_PATH_LITERAL("android_demo_apps.squash");

constexpr base::FilePath::CharType kExternalExtensionsPrefsPath[] =
    FILE_PATH_LITERAL("demo_extensions.json");

}  // namespace

// static
const char DemoResources::kDemoModeResourcesComponentName[] =
    "demo-mode-resources";

// static
const char DemoResources::kOfflineDemoModeResourcesComponentName[] =
    "offline-demo-mode-resources";

// static
base::FilePath DemoResources::GetPreInstalledPath() {
  base::FilePath preinstalled_components_root;
  base::PathService::Get(DIR_PREINSTALLED_COMPONENTS,
                         &preinstalled_components_root);
  return preinstalled_components_root.AppendASCII("cros-components")
      .AppendASCII(kOfflineDemoModeResourcesComponentName);
}

DemoResources::DemoResources(DemoSession::DemoModeConfig config)
    : config_(config) {
  DCHECK_NE(config_, DemoSession::DemoModeConfig::kNone);
}

DemoResources::~DemoResources() = default;

base::FilePath DemoResources::GetAbsolutePath(
    const base::FilePath& relative_path) const {
  if (path_.empty())
    return base::FilePath();
  if (relative_path.ReferencesParent())
    return base::FilePath();
  return path_.Append(relative_path);
}

base::FilePath DemoResources::GetDemoAppsPath() const {
  if (path_.empty())
    return base::FilePath();
  return path_.Append(kDemoAppsPath);
}

base::FilePath DemoResources::GetExternalExtensionsPrefsPath() const {
  if (path_.empty())
    return base::FilePath();
  return path_.Append(kExternalExtensionsPrefsPath);
}

void DemoResources::EnsureLoaded(base::OnceClosure load_callback) {
  if (loaded_) {
    if (load_callback)
      std::move(load_callback).Run();
    return;
  }

  if (load_callback)
    load_callbacks_.emplace_back(std::move(load_callback));

  if (load_requested_)
    return;
  load_requested_ = true;

  if (config_ == DemoSession::DemoModeConfig::kOffline) {
    LoadPreinstalledOfflineResources();
    return;
  }

  component_updater::CrOSComponentManager* cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  // In unit tests, DemoModeTestHelper should set up a fake
  // CrOSComponentManager.
  DCHECK(cros_component_manager);

  g_browser_process->platform_part()->cros_component_manager()->Load(
      kDemoModeResourcesComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&DemoResources::InstalledComponentLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoResources::SetCrOSComponentLoadedForTesting(
    const base::FilePath& path,
    component_updater::CrOSComponentManager::Error error) {
  InstalledComponentLoaded(error, path);
}

void DemoResources::SetPreinstalledOfflineResourcesLoadedForTesting(
    const base::FilePath& path) {
  OnDemoResourcesLoaded(path);
}

void DemoResources::InstalledComponentLoaded(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  component_error_ = error;
  OnDemoResourcesLoaded(base::make_optional(path));
}

void DemoResources::LoadPreinstalledOfflineResources() {
  chromeos::DBusThreadManager::Get()
      ->GetImageLoaderClient()
      ->LoadComponentAtPath(
          kOfflineDemoModeResourcesComponentName, GetPreInstalledPath(),
          base::BindOnce(&DemoResources::OnDemoResourcesLoaded,
                         weak_ptr_factory_.GetWeakPtr()));
}

void DemoResources::OnDemoResourcesLoaded(
    base::Optional<base::FilePath> mounted_path) {
  loaded_ = true;

  if (mounted_path.has_value())
    path_ = mounted_path.value();

  std::list<base::OnceClosure> load_callbacks;
  load_callbacks.swap(load_callbacks_);
  for (auto& callback : load_callbacks)
    std::move(callback).Run();
}

}  // namespace chromeos
