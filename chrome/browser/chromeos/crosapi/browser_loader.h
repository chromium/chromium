// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_LOADER_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/component_updater/cros_component_manager.h"

namespace crosapi {

// Manages download of the lacros-chrome binary. This class is a part of
// ash-chrome.
class BrowserLoader {
 public:
  explicit BrowserLoader(
      scoped_refptr<component_updater::CrOSComponentManager> manager);

  BrowserLoader(const BrowserLoader&) = delete;
  BrowserLoader& operator=(const BrowserLoader&) = delete;

  ~BrowserLoader();

  // Starts to load lacros-chrome binary.
  // |callback| is called on completion with the path to the lacros-chrome on
  // success, or an empty filepath on failure.
  using LoadCompletionCallback =
      base::OnceCallback<void(const base::FilePath&)>;
  void Load(LoadCompletionCallback callback);

  // Starts to unload lacros-chrome binary.
  // Note that this triggers to remove the user directory for lacros-chrome.
  void Unload();

 private:
  // Called on the completion of loading.
  void OnLoadComplete(LoadCompletionCallback callback,
                      component_updater::CrOSComponentManager::Error error,
                      const base::FilePath& path);

  // Unloading hops threads. This is called after we check whether Lacros was
  // installed and maybe clean up the user directory.
  void OnCheckInstalled(bool was_installed);

  // Unloads the component. Called after system salt is available.
  void UnloadAfterCleanUp(const std::string& ignored_salt);

  // May be null in tests.
  scoped_refptr<component_updater::CrOSComponentManager> component_manager_;

  base::WeakPtrFactory<BrowserLoader> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_CHROMEOS_CROSAPI_BROWSER_LOADER_H_
