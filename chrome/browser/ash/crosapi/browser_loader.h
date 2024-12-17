// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_
#define CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/version.h"
#include "chromeos/ash/components/standalone_browser/lacros_selection.h"

namespace component_updater {
class ComponentManagerAsh;
}  // namespace component_updater

namespace crosapi {

class LacrosSelectionLoader;
class LacrosSelectionLoaderFactory;
using ash::standalone_browser::LacrosSelection;

// Manages download of the lacros-chrome binary.
// This class is a part of ash-chrome.
class BrowserLoader {
 public:
  explicit BrowserLoader(
      scoped_refptr<component_updater::ComponentManagerAsh> manager);

  // Constructor for testing.
  explicit BrowserLoader(std::unique_ptr<LacrosSelectionLoaderFactory> factory);

  BrowserLoader(const BrowserLoader&) = delete;
  BrowserLoader& operator=(const BrowserLoader&) = delete;

  virtual ~BrowserLoader();

  virtual void Unload();

 private:
  // Called on unload completed.
  void OnUnloadCompleted(LacrosSelection selection);

  // Loader for rootfs lacros and stateful lacros. Loader objects are
  // constructed on start loading and reset on unload completion or on reload.
  std::unique_ptr<LacrosSelectionLoader> rootfs_lacros_loader_;
  std::unique_ptr<LacrosSelectionLoader> stateful_lacros_loader_;

  std::unique_ptr<LacrosSelectionLoaderFactory> factory_;

  // Called when Unload is completed for both rootfs and stateful lacros.
  base::OnceClosure callback_on_unload_completion_;

  // Used for DCHECKs to ensure method calls executed in the correct thread.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<BrowserLoader> weak_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_BROWSER_LOADER_H_
