// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_
#define CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/version.h"
#include "chrome/common/child_module/child_module_helper.h"

namespace child_module {

class ChildModuleWatcher;

// Manages available dynamically-staged child modules (such as patched renderer
// binaries). Owned by GlobalFeatures on desktop platforms. Confined to the UI
// thread.
class ChildModuleManager {
 public:
  // Constructs the manager. Manages the default child modules directory
  // (GetModulesDir()).
  ChildModuleManager();
  ChildModuleManager(const ChildModuleManager&) = delete;
  ChildModuleManager& operator=(const ChildModuleManager&) = delete;
  ~ChildModuleManager();

  // Returns the latest available child module version, or std::nullopt if no
  // valid patched version is currently available.
  std::optional<base::Version> GetLatestVersion() const;

  // Returns the path to the renderer binary for `version` if that version is
  // currently available; otherwise returns an empty FilePath.
  base::FilePath GetRendererBinaryPath(const base::Version& version) const;

  // Returns the set of all currently available versions, sorted descending.
  const VersionSet& GetAvailableVersions() const;

  // Blocks the calling sequence until the initial background directory scan
  // and subsequent notification to the manager have completed.
  void WaitForInitialScanForTesting();

 private:
  void OnVersionSetChanged(VersionSet versions);

  VersionSet available_versions_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::SequenceBound<ChildModuleWatcher> watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ChildModuleManager> weak_factory_{this};
};

}  // namespace child_module

#endif  // CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_
