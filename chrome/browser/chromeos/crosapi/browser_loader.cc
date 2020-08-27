// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crosapi/browser_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/chromeos/crosapi/browser_util.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/cryptohome/system_salt_getter.h"

namespace crosapi {

namespace {

constexpr char kLacrosComponentName[] = "lacros-fishfood";

// Returns whether lacros-fishfood component is already installed.
// If it is, delete the user directory, too, because it will be
// uninstalled.
bool CheckInstalledAndMaybeRemoveUserDirectory(
    scoped_refptr<component_updater::CrOSComponentManager> manager) {
  if (!manager->IsRegisteredMayBlock(kLacrosComponentName))
    return false;

  // Since we're already on a background thread, delete the user-data-dir
  // associated with lacros.
  // TODO(hidehiko): This approach has timing issue. Specifically, if Chrome
  // shuts down during the directory remove, some partially-removed directory
  // may be kept, and if the user flips the flag in the next time, that
  // partially-removed directory could be used. Fix this.
  base::DeletePathRecursively(browser_util::GetUserDataDir());
  return true;
}

}  // namespace

BrowserLoader::BrowserLoader(
    scoped_refptr<component_updater::CrOSComponentManager> manager)
    : component_manager_(manager) {
  DCHECK(component_manager_);
}

BrowserLoader::~BrowserLoader() = default;

void BrowserLoader::Load(LoadCompletionCallback callback) {
  DCHECK(browser_util::IsLacrosAllowed());

  // TODO(crbug.com/1078607): Remove non-error logging from this class.
  LOG(WARNING) << "Starting lacros component load.";

  // If the user has specified a path for the lacros-chrome binary, use that
  // rather than component manager.
  base::FilePath lacros_chrome_path =
      base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
          chromeos::switches::kLacrosChromePath);
  if (!lacros_chrome_path.empty()) {
    OnLoadComplete(std::move(callback),
                   component_updater::CrOSComponentManager::Error::NONE,
                   lacros_chrome_path);
    return;
  }

  component_manager_->Load(
      kLacrosComponentName,
      component_updater::CrOSComponentManager::MountPolicy::kMount,
      component_updater::CrOSComponentManager::UpdatePolicy::kForce,
      base::BindOnce(&BrowserLoader::OnLoadComplete, weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void BrowserLoader::Unload() {
  DCHECK(browser_util::IsLacrosAllowed());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CheckInstalledAndMaybeRemoveUserDirectory,
                     component_manager_),
      base::BindOnce(&BrowserLoader::OnCheckInstalled,
                     weak_factory_.GetWeakPtr()));
}

void BrowserLoader::OnLoadComplete(
    LoadCompletionCallback callback,
    component_updater::CrOSComponentManager::Error error,
    const base::FilePath& path) {
  bool success =
      (error == component_updater::CrOSComponentManager::Error::NONE);
  if (success) {
    LOG(WARNING) << "Loaded lacros image at " << path.MaybeAsASCII();
  } else {
    LOG(WARNING) << "Error loading lacros component image: "
                 << static_cast<int>(error);
  }
  std::move(callback).Run(success ? path : base::FilePath());
}

void BrowserLoader::OnCheckInstalled(bool was_installed) {
  if (!was_installed)
    return;

  // Workaround for login crash when the user un-sets the LacrosSupport flag.
  // CrOSComponentManager::Unload() calls into code in MetadataTable that
  // assumes that system salt is available. This isn't always true when chrome
  // restarts to apply non-owner flags. It's hard to make MetadataTable async.
  // Ensure salt is available before unloading. https://crbug.com/1122674
  chromeos::SystemSaltGetter::Get()->GetSystemSalt(base::BindOnce(
      &BrowserLoader::UnloadAfterCleanUp, weak_factory_.GetWeakPtr()));
}

void BrowserLoader::UnloadAfterCleanUp(const std::string& ignored_salt) {
  CHECK(chromeos::SystemSaltGetter::Get()->GetRawSalt());
  component_manager_->Unload(kLacrosComponentName);
}

}  // namespace crosapi
