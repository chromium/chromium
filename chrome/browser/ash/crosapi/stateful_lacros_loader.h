// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_

#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "chrome/browser/component_updater/cros_component_manager.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      component_updater::ComponentUpdateService* updater,
      const std::string& lacros_component_name);
  StatefulLacrosLoader(const StatefulLacrosLoader&) = delete;
  StatefulLacrosLoader& operator=(const StatefulLacrosLoader&) = delete;
  ~StatefulLacrosLoader() override;

  // LacrosSelectionLoader:
  void Load(LoadCompletionCallback callback, bool forced) override;
  void Unload() override;
  void Reset() override;
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;

 private:
  void LoadInternal(LoadCompletionCallback callback, bool forced);

  // Returns true if the stateful lacros-chrome is already loaded and both
  // `version_` and `path_` are ready.
  bool IsReady();

  // Called after Load.
  void OnLoad(LoadCompletionCallback callback,
              component_updater::CrOSComponentManager::Error error,
              const base::FilePath& path);

  // Called in GetVersion sequence on IsInstalledMayBlock returns result.
  void OnCheckInstalledToGetVersion(
      base::OnceCallback<void(const base::Version&)> callback,
      bool is_installed);

  // Called after gettin version from CrOSComponentManager::GetVersion.
  void OnGetVersionFromComponentManager(
      base::OnceCallback<void(const base::Version&)> callback,
      const base::Version& version);

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
  // Cache the path to installed lacros-chrome path.
  absl::optional<base::FilePath> path_;

  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  // May be null in tests.
  const raw_ptr<component_updater::ComponentUpdateService, ExperimentalAsh>
      component_update_service_;

  const std::string lacros_component_name_;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StatefulLacrosLoader> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_
