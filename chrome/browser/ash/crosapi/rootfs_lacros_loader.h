// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ROOTFS_LACROS_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_ROOTFS_LACROS_LOADER_H_

#include <optional>
#include <ostream>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"

namespace ash {
class UpstartClient;
}  // namespace ash

namespace crosapi {

class RootfsLacrosLoader : public LacrosSelectionLoader {
 public:
  // Constructor for production.
  RootfsLacrosLoader();
  // Constructor for testing.
  explicit RootfsLacrosLoader(ash::UpstartClient* upstart_client,
                              base::FilePath metadata_path);
  RootfsLacrosLoader(const RootfsLacrosLoader&) = delete;
  RootfsLacrosLoader& operator=(const RootfsLacrosLoader&) = delete;
  ~RootfsLacrosLoader() override;

  // The state representing the loading state.
  enum class State {
    // Loader is not running any task.
    kNotLoaded,

    // Loader is in the process of reading the version from manifest file.
    kReadingVersion,

    // Loader gets version of lacros-chrome but not loaded it yet.
    kVersionReadyButNotLoaded,

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
  void GetVersionInternal(
      base::OnceCallback<void(const base::Version&)> callback);

  // Called after GetVersion.
  void OnGetVersion(base::OnceCallback<void(const base::Version&)> callback,
                    base::Version version);

  // Called when `version_` is calculated and set during Load() sequence.
  void OnVersionReadyToLoad(LoadCompletionCallback callback,
                            const base::Version& version);

  // Called on checking rootfs lacros-chrome is already maounted or not during
  // Load() sequence.
  void OnMountCheckToLoad(LoadCompletionCallback callback,
                          bool already_mounted);

  // Callback from upstart mounting lacros-chrome.
  void OnUpstartLacrosMounter(LoadCompletionCallback callback, bool success);

  // Called on unload completed.
  void OnUnloadCompleted(base::OnceClosure callback, bool success);

  // The bundled rootfs lacros-chrome binary version. This is set after the
  // first async call that checks the installed rootfs lacros version number.
  // If `version_` is null, it implies the version is not yet calculated.
  // For cases where it failed to read the version, invalid `base::Version()` is
  // set.
  std::optional<base::Version> version_;

  // Pointer held to `UpstartClient` for testing purposes.
  // Otherwise, the lifetime is the same as `ash::UpstartClient::Get()`.
  const raw_ptr<ash::UpstartClient> upstart_client_;

  // The path which stores the metadata including the version.
  // This is always the same for production code, but may be overridden on
  // testing.
  base::FilePath metadata_path_;

  base::OnceClosure pending_unload_;

  State state_ = State::kNotLoaded;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RootfsLacrosLoader> weak_factory_{this};
};

std::ostream& operator<<(std::ostream&, RootfsLacrosLoader::State);

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ROOTFS_LACROS_LOADER_H_
