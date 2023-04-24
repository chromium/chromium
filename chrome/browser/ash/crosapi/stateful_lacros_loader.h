// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class FilePath;
}  // namespace base

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace crosapi {

class StatefulLacrosLoader : public LacrosSelectionLoader {
 public:
  // Constructor for production.
  explicit StatefulLacrosLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager);
  // Constructor for testing.
  explicit StatefulLacrosLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager,
      component_updater::ComponentUpdateService* updater);
  StatefulLacrosLoader(const StatefulLacrosLoader&) = delete;
  StatefulLacrosLoader& operator=(const StatefulLacrosLoader&) = delete;
  ~StatefulLacrosLoader() override;

  // LacrosSelectionLoader:
  void Load(LoadCompletionCallback callback) override;
  void Unload() override;
  void Reset() override;
  void GetVersion(base::OnceCallback<void(base::Version)> callback) override;

 private:
  // Called in GetVersion sequence on IsInstalledMayBlock returns result.
  void OnCheckInstalledToGetVersion(
      base::OnceCallback<void(base::Version)> callback,
      bool is_installed);

  void OnLoad(LoadCompletionCallback callback,
              component_updater::CrOSComponentManager::Error error,
              const base::FilePath& path);

  // Called in Unload sequence.
  // Unloading hops threads. This is called after we check whether Lacros was
  // installed and maybe clean up the user directory.
  void OnCheckInstalledToUnload(bool was_installed);

  // Unloads the component. Called after system salt is available.
  void UnloadAfterCleanUp(const std::string& ignored_salt);

  // If `version_` is null, it implies the version is not yet calculated.
  // For cases where it failed to read the version, invalid `base::Version()` is
  // set.
  absl::optional<base::Version> version_;

  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  // May be null in tests.
  const raw_ptr<component_updater::ComponentUpdateService, ExperimentalAsh>
      component_update_service_;

  base::WeakPtrFactory<StatefulLacrosLoader> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_
