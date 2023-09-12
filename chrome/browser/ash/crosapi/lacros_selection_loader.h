// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_H_

#include "base/functional/callback.h"
#include "chrome/browser/ash/crosapi/browser_loader.h"

namespace base {
class FilePath;
class Version;
}  // namespace base

namespace crosapi {

// LacrosSelectionLoader is a class to handle rootfs/stateful lacros-chrome.
class LacrosSelectionLoader {
 public:
  virtual ~LacrosSelectionLoader() = default;

  // The name of lacros-chrome binary file. Attached to the directory path.
  static constexpr char kLacrosChromeBinary[] = "chrome";

  // Returns version and directory path of lacros-chrome binary to load.
  // If any error occurs during loading, it returns empty version and path.
  using LoadCompletionCallback =
      base::OnceCallback<void(base::Version, const base::FilePath&)>;

  // Loads chrome binary.
  // `forced` specifies whether the lacros selection is forced.
  virtual void Load(LoadCompletionCallback callback, bool forced) = 0;

  // Unloads chrome binary.
  virtual void Unload() = 0;

  // Resets the state. This is called before reloading lacros.
  // TODO(elkurin): Instead of resetting the state, throw away and recreate
  // loader instance.
  virtual void Reset() = 0;

  // Calculates version and send it back via `callback`.
  // It may take time since it requires to load/mount lacros binary.
  virtual void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) = 0;

  // Sets version.
  // Only implemented for testing class (FakeLacrosSelectionLoader).
  virtual void SetVersionForTesting(const base::Version& version) {}
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_H_
