// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/demo_mode/demo_components.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace {

// Path relative to the path at which demo resources are loaded that
// contains image with demo Android apps.
constexpr base::FilePath::CharType kDemoAndroidAppsPath[] =
    FILE_PATH_LITERAL("android_demo_apps.squash");

constexpr base::FilePath::CharType kExternalExtensionsPrefsPath[] =
    FILE_PATH_LITERAL("demo_extensions.json");

PrefService* LocalState() {
  if (!g_browser_process) {
    return nullptr;
  }

  return g_browser_process->local_state();
}

void RecordAppVersion(const base::Version& version) {
  auto* local_state = LocalState();
  // In some unittests `local_state` may be null.
  if (local_state) {
    local_state->SetString(prefs::kDemoModeAppVersion, version.IsValid()
                                                           ? version.GetString()
                                                           : std::string());
  }
}

void RecordResourcesVersion(const base::Version& version) {
  auto* local_state = LocalState();
  // In some unittests `local_state` may be null.
  if (local_state) {
    local_state->SetString(
        prefs::kDemoModeResourcesVersion,
        version.IsValid() ? version.GetString() : std::string());
  }
}

}  // namespace

// static
const char DemoComponents::kDemoModeResourcesComponentName[] =
    "demo-mode-resources";

const char DemoComponents::kDemoModeAppComponentName[] = "demo-mode-app";

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
  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kDemoModeSwaContentDirectory)) {
    OnAppComponentLoaded(std::move(load_callback),
                         component_updater::ComponentManagerAsh::Error::NONE,
                         base::FilePath(command_line->GetSwitchValueASCII(
                             ash::switches::kDemoModeSwaContentDirectory)));
    return;
  }

  g_browser_process->platform_part()->component_manager_ash()->Load(
      kDemoModeAppComponentName,
      component_updater::ComponentManagerAsh::MountPolicy::kMount,
      component_updater::ComponentManagerAsh::UpdatePolicy::kDontForce,
      base::BindOnce(&DemoComponents::OnAppComponentLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(load_callback)));
}

void DemoComponents::OnAppComponentLoaded(
    base::OnceClosure load_callback,
    component_updater::ComponentManagerAsh::Error error,
    const base::FilePath& app_component_path) {
  // Before returning saying that the app component has been loaded
  // let's ensure that the app's version is loaded.
  app_component_error_ = error;
  default_app_component_path_ = app_component_path;

  auto component_manager_ash =
      g_browser_process->platform_part()->component_manager_ash();

  // component_manager_ash may be null in unit tests.
  if (component_manager_ash) {
    component_manager_ash->GetVersion(
        kDemoModeAppComponentName,
        base::BindOnce(&DemoComponents::OnAppVersionReady,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(load_callback)));
  } else {
    std::move(load_callback).Run();
  }
}

void DemoComponents::LoadResourcesComponent(base::OnceClosure load_callback) {
  // TODO(b/254735031): Consider removing this callback queuing logic, since
  // it's already supported internally by ComponentManagerAsh::Load
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

  const auto* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kDemoModeResourceDirectory)) {
    InstalledComponentLoaded(
        component_updater::ComponentManagerAsh::Error::NONE,
        base::FilePath(command_line->GetSwitchValueASCII(
            ash::switches::kDemoModeResourceDirectory)));
    return;
  }

  auto component_manager_ash =
      g_browser_process->platform_part()->component_manager_ash();
  // In unit tests, DemoModeTestHelper should set up a fake
  // ComponentManagerAsh.
  DCHECK(component_manager_ash);

  component_manager_ash->Load(
      kDemoModeResourcesComponentName,
      component_updater::ComponentManagerAsh::MountPolicy::kMount,
      component_updater::ComponentManagerAsh::UpdatePolicy::kDontForce,
      base::BindOnce(&DemoComponents::InstalledComponentLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DemoComponents::OnAppVersionReady(base::OnceClosure callback,
                                       const base::Version& version) {
  app_component_version_ = version;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RecordAppVersion, version));

  std::move(callback).Run();
}

void DemoComponents::OnResourcesVersionReady(const base::FilePath& path,
                                             const base::Version& version) {
  resources_component_version_ = version;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&RecordResourcesVersion, version));

  OnDemoResourcesLoaded(std::make_optional(path));
}

void DemoComponents::SetCrOSComponentLoadedForTesting(
    const base::FilePath& path,
    component_updater::ComponentManagerAsh::Error error) {
  InstalledComponentLoaded(error, path);
  OnAppComponentLoaded(base::DoNothing(), error, path);
}

void DemoComponents::SetPreinstalledOfflineResourcesLoadedForTesting(
    const base::FilePath& path) {
  OnDemoResourcesLoaded(path);
}

void DemoComponents::InstalledComponentLoaded(
    component_updater::ComponentManagerAsh::Error error,
    const base::FilePath& path) {
  resources_component_error_ = error;

  auto component_manager_ash =
      g_browser_process->platform_part()->component_manager_ash();

  // component_manager_ash may be null in unit tests.
  if (component_manager_ash) {
    component_manager_ash->GetVersion(
        kDemoModeResourcesComponentName,
        base::BindOnce(&DemoComponents::OnResourcesVersionReady,
                       weak_ptr_factory_.GetWeakPtr(), path));
  } else {
    OnDemoResourcesLoaded(std::make_optional(path));
  }
}

void DemoComponents::OnDemoResourcesLoaded(
    std::optional<base::FilePath> mounted_path) {
  resources_loaded_ = true;

  if (mounted_path.has_value())
    resources_component_path_ = mounted_path.value();

  std::list<base::OnceClosure> load_callbacks;
  load_callbacks.swap(load_callbacks_);
  for (auto& callback : load_callbacks)
    std::move(callback).Run();
}

}  // namespace ash
