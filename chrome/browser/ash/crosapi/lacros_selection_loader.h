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

  // Loads chrome binary. This must be called only when LacrosSelectionLoader is
  // idle and has not yet unloaded.
  // `forced` specifies whether the lacros selection is forced.
  virtual void Load(LoadCompletionCallback callback, bool forced) = 0;

  // Unloads chrome binary. Unload can be called anytime, and this request will
  // stop other tasks. Once Unload is called, LacrosSelectionLoader is no longer
  // valid.
  // `callback` will be called on unload completion.
  virtual void Unload(base::OnceClosure callback) = 0;

  // Returns the current unloading/unloaded state.
  virtual bool IsUnloading() const = 0;
  virtual bool IsUnloaded() const = 0;

  // Calculates version and send it back via `callback`.
  // It may take time since it requires to load/mount lacros binary.
  virtual void GetVersion(
      base::OnceCallback<void(const base::Version&)> callback) = 0;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_LACROS_SELECTION_LOADER_H_
