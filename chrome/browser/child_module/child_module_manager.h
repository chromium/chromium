// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_
#define CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/version.h"
#include "chrome/browser/child_module/version_set.h"

class PrefRegistrySimple;
class PrefService;

namespace child_module {

class ChildModuleWatcher;

// Manages available dynamically-staged child modules (such as patched renderer
// binaries). Owned by GlobalFeatures on desktop platforms. Confined to the UI
// thread.
class ChildModuleManager {
 public:
  // A child module that is available for use.
  struct Module {
    // The version number of the child module.
    base::Version version;

    // The full path to the renderer binary within the child module.
    base::FilePath renderer_binary_path;

    friend bool operator==(const Module&, const Module&) = default;
  };

  // Registers local-state preferences used by ChildModuleManager.
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Constructs the manager and starts monitoring the default child modules
  // directory (`GetModulesDir()`). Staged modules are served when
  // `DynamicPatchingEnabled` is enabled in `local_state`. `local_state` must
  // outlive this instance.
  explicit ChildModuleManager(PrefService& local_state);
  ChildModuleManager(const ChildModuleManager&) = delete;
  ChildModuleManager& operator=(const ChildModuleManager&) = delete;
  ~ChildModuleManager();

  // Returns the latest available child module, or std::nullopt if dynamic
  // patching is disabled by policy or if no valid patched version is currently
  // available.
  std::optional<Module> GetLatestModule() const;

  // Returns the set of all currently discovered versions, sorted descending.
  const VersionSet& GetAvailableVersionsForTesting() const;

  // Blocks the calling sequence until the background directory scan and
  // subsequent notification to the manager have completed.
  void WaitForInitialScanForTesting();

 private:
  bool IsDynamicPatchingEnabled() const
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnVersionSetChanged(VersionSet versions);

  const raw_ref<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);
  VersionSet available_versions_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::SequenceBound<ChildModuleWatcher> watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ChildModuleManager> weak_factory_{this};
};

}  // namespace child_module

#endif  // CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_
