// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_

#include <optional>
#include <ostream>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "components/component_updater/ash/component_manager_ash.h"

namespace component_updater {
class ComponentUpdateService;
}  // namespace component_updater

namespace crosapi {

class StatefulLacrosLoader : public LacrosSelectionLoader {
 public:
  // Constructor for production.
  explicit StatefulLacrosLoader(
      scoped_refptr<component_updater::ComponentManagerAsh> manager);
  // Constructor for testing.
  explicit StatefulLacrosLoader(
      scoped_refptr<component_updater::ComponentManagerAsh> manager,
      component_updater::ComponentUpdateService* updater,
      const std::string& lacros_component_name);
  StatefulLacrosLoader(const StatefulLacrosLoader&) = delete;
  StatefulLacrosLoader& operator=(const StatefulLacrosLoader&) = delete;
  ~StatefulLacrosLoader() override;

  // The state representing the loading state.
  enum class State {
    // Loader is not running any task.
    kNotLoaded,

    // Loader is in the process of loading lacros-chrome.
    kLoading,

    // Loader has loaded lacros-chrome and `version_` and `path_` is ready.
    kLoaded,

    // Loader is in the process of unloading lacros-chrome.
    kUnloading,

    // Loader has unloaded the lacros-chrome. State must NOT change once it
    // becomes kUnloaded.
    kUnloaded,
  };

  State GetState() const { return state_; }

  // LacrosSelectionLoader:
  void Load(LoadCompletionCallback callback, bool forced) override;
  void Unload(base::OnceClosure callback) override;
  void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) override;
  bool IsUnloading() const override;
  bool IsUnloaded() const override;

 private:
  void LoadInternal(LoadCompletionCallback callback, bool forced);

  // Called after Load.
  void OnLoad(LoadCompletionCallback callback,
              component_updater::ComponentManagerAsh::Error error,
              const base::FilePath& path);

  // Called in GetVersion sequence on IsInstalledMayBlock returns result.
  void OnCheckInstalledToGetVersion(
      base::OnceCallback<void(const base::Version&)> callback,
      bool is_installed);

  // Called in Unload sequence.
  // Unloading hops threads. This is called after we check whether Lacros was
  // installed and maybe clean up the user directory.
  void OnCheckInstalledToUnload(base::OnceClosure callback, bool was_installed);

  // Unloads the component. Called after system salt is available.
  void UnloadAfterCleanUp(base::OnceClosure callback,
                          const std::string& ignored_salt);

  // If `version_` is null, it implies the version is not yet calculated.
  // For cases where it failed to read the version, invalid `base::Version()` is
  // set.
  std::optional<base::Version> version_;
  // Cache the path to installed lacros-chrome path.
  std::optional<base::FilePath> path_;

  scoped_refptr<component_updater::ComponentManagerAsh> component_manager_;

  // May be null in tests.
  const raw_ptr<component_updater::ComponentUpdateService>
      component_update_service_;

  const std::string lacros_component_name_;

  base::OnceClosure pending_unload_;

  State state_ = State::kNotLoaded;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<StatefulLacrosLoader> weak_factory_{this};
};

std::ostream& operator<<(std::ostream&, StatefulLacrosLoader::State);

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_STATEFUL_LACROS_LOADER_H_
