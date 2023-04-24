// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_ROOTFS_LACROS_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_ROOTFS_LACROS_LOADER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "chrome/browser/ash/crosapi/lacros_selection_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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

  // LacrosSelectionLoader:
  void Load(LoadCompletionCallback callback) override;
  void Unload() override;
  void Reset() override;
  void GetVersion(base::OnceCallback<void(base::Version)> callback) override;

 private:
  // Called after GetVersion.
  void OnGetVersion(base::OnceCallback<void(base::Version)> callback,
                    base::Version version);

  // Called when `version_` is calculated and set during Load() sequence.
  void OnVersionReadyToLoad(LoadCompletionCallback callback,
                            base::Version version);

  // Called on checking rootfs lacros-chrome is already maounted or not during
  // Load() sequence.
  void OnMountCheckToLoad(LoadCompletionCallback callback,
                          bool already_mounted);

  // Callback from upstart mounting lacros-chrome.
  void OnUpstartLacrosMounter(LoadCompletionCallback callback, bool success);

  // The bundled rootfs lacros-chrome binary version. This is set after the
  // first async call that checks the installed rootfs lacros version number.
  // If `version_` is null, it implies the version is not yet calculated.
  // For cases where it failed to read the version, invalid `base::Version()` is
  // set.
  absl::optional<base::Version> version_;

  // Pointer held to `UpstartClient` for testing purposes.
  // Otherwise, the lifetime is the same as `ash::UpstartClient::Get()`.
  const raw_ptr<ash::UpstartClient, ExperimentalAsh> upstart_client_;

  // The path which stores the metadata including the version.
  // This is always the same for production code, but may be overridden on
  // testing.
  base::FilePath metadata_path_;

  base::WeakPtrFactory<RootfsLacrosLoader> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_ROOTFS_LACROS_LOADER_H_
