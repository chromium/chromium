// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_components.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_paths.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"

namespace ash {
namespace {

// Path relative to the path at which demo resources are loaded that
// contains image with demo Android apps.
constexpr base::FilePath::CharType kDemoAndroidAppsPath[] =
    FILE_PATH_LITERAL("android_demo_apps.squash");

constexpr base::FilePath::CharType kExternalExtensionsPrefsPath[] =
    FILE_PATH_LITERAL("demo_extensions.json");

}  // namespace

// static
const char DemoComponents::kDemoModeResourcesComponentName[] =
    "demo-mode-resources";

const char DemoComponents::kDemoModeAppComponentName[] = "demo-mode-app";

// static
const char DemoComponents::kOfflineDemoModeResourcesComponentName[] =
    "offline-demo-mode-resources";

// static
base::FilePath DemoComponents::GetPreInstalledPath() {
  base::FilePath preinstalled_components_root;
  base::PathService::Get(DIR_PREINSTALLED_COMPONENTS,
                         &preinstalled_components_root);
  return preinstalled_components_root.AppendASCII("cros-components")
      .AppendASCII(kOfflineDemoModeResourcesComponentName);
}

DemoComponents::DemoComponents(DemoSession::DemoModeConfig config)
    : config_(config) {
  DCHECK_NE(config_, DemoSession::DemoModeConfig::kNone);
}

DemoComponents::~DemoComponents() = default;

base::FilePath DemoComponents::GetAbsolutePath(
    const base::FilePath& relative_path) const {
  if (resources_component_path_.empty())
    return base::FilePath();
  if (relative_path.ReferencesParent())
    return base::FilePath();
  return resources_component_path_.Append(relative_path);
}

base::FilePath DemoComponents::GetDemoAndroidAppsPath() const {
  if (resources_component_path_.empty())
    return base::FilePath();
  return resources_component_path_.Append(kDemoAndroidAppsPath);
}

base::FilePath DemoComponents::GetExternalExtensionsPrefsPath() const {
  if (resources_component_path_.empty())
    return base::FilePath();
  return resources_component_path_.Append(kExternalExtensionsPrefsPath);
}

void DemoComponents::LoadAppComponent(base::OnceClosure load_callback) {
  g_browser_process->platform_part()->cros_component_manager()->Load(
      kDemoModeAppComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&DemoComponents::OnAppComponentLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(load_callback)));
}

void DemoComponents::OnAppComponentLoaded(
    base::OnceClosure load_callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& app_component_path) {
  app_component_error_ = error;
  default_app_component_path_ = app_component_path;
  std::move(load_callback).Run();
}

void DemoComponents::LoadResourcesComponent(base::OnceClosure load_callback) {
  // TODO(b/254735031): Consider removing this callback queuing logic, since
  // it's already supported internally by CrOSComponentManager::Load
  if (resources_loaded_) {
    if (load_callback)
      std::move(load_callback).Run();
    return;
  }

  if (load_callback)
    load_callbacks_.emplace_back(std::move(load_callback));

  if (resources_load_requested_)
    return;
  resources_load_requested_ = true;

  auto cros_component_manager =
      g_browser_process->platform_part()->cros_component_manager();
  // In unit tests, DemoModeTestHelper should set up a fake
  // CrOSComponentManager.
  DCHECK(cros_component_manager);

  cros_component_manager->Load(
      kDemoModeResourcesComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kDontForce,
      base::BindOnce(&DemoComponents::InstalledComponentLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoComponents::SetCrOSComponentLoadedForTesting(
    const base::FilePath& path,
    component_updater::CrOSComponentManager::Error error) {
  InstalledComponentLoaded(error, path);
  OnAppComponentLoaded(base::DoNothing(), error, path);
}

void DemoComponents::SetPreinstalledOfflineResourcesLoadedForTesting(
    const base::FilePath& path) {
  OnDemoResourcesLoaded(path);
}

void DemoComponents::InstalledComponentLoaded(
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  resources_component_error_ = error;
  OnDemoResourcesLoaded(absl::make_optional(path));
}

void DemoComponents::OnDemoResourcesLoaded(
    absl::optional<base::FilePath> mounted_path) {
  resources_loaded_ = true;

  if (mounted_path.has_value())
    resources_component_path_ = mounted_path.value();

  std::list<base::OnceClosure> load_callbacks;
  load_callbacks.swap(load_callbacks_);
  for (auto& callback : load_callbacks)
    std::move(callback).Run();
}

}  // namespace ash
