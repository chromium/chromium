// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_
#define CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/process/kill.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/version.h"
#include "chrome/browser/child_module/version_set.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/child_process_id.h"

class PrefRegistrySimple;
class PrefService;

namespace content {
class RenderProcessHost;
struct ChildProcessTerminationInfo;
}  // namespace content

namespace child_module {

class ChildModuleWatcher;

// Manages available dynamically-staged child modules (such as patched renderer
// binaries). Monitors child process creation, tracks consecutive crash loops
// through an automated circuit breaker, and persists blocked versions to
// Local State. Owned by GlobalFeatures on desktop platforms. Confined to the UI
// thread.
class ChildModuleManager : public content::RenderProcessHostObserver {
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

  // Constructs the manager and starts monitoring the directory in which child
  // module versions for the running browser are staged. No versions are
  // available if the directory was not set. Staged modules are served when
  // `DynamicPatchingEnabled` is enabled in `local_state`. `local_state` must
  // outlive this instance.
  explicit ChildModuleManager(PrefService& local_state);
  ChildModuleManager(const ChildModuleManager&) = delete;
  ChildModuleManager& operator=(const ChildModuleManager&) = delete;
  ~ChildModuleManager() override;

  // Selects the latest available non-blocked child module for the
  // RenderProcessHost identified by `render_process_id`, associates it with the
  // host (or clears any previous association if none is available), and begins
  // observing the host for circuit breaker crash tracking. Returns the selected
  // module, or std::nullopt if no such host exists, if no patched module is
  // available, or if all available versions are blocked. The returned
  // reference remains valid until the RenderProcessHost is destroyed, exits, or
  // SelectModuleForProcess() is called again for the same host.
  base::optional_ref<const Module> SelectModuleForProcess(
      content::ChildProcessId render_process_id);

  // Returns the child module associated with the RenderProcessHost identified
  // by `render_process_id` by a prior call to SelectModuleForProcess(), or
  // std::nullopt if no such host exists or if it is not using a patched module.
  // The returned reference remains valid until the RenderProcessHost is
  // destroyed, exits, or SelectModuleForProcess() is called again for the same
  // host.
  base::optional_ref<const Module> GetModuleForProcess(
      content::ChildProcessId render_process_id) const;

  // Returns the set of all currently discovered versions, sorted descending.
  const VersionSet& GetAvailableVersionsForTesting() const;

  // Returns the set of currently blocked versions.
  const base::flat_set<base::Version>& GetBlockedVersionsForTesting() const;

  // Blocks the calling sequence until the background directory scan and
  // subsequent notification to the manager have completed.
  void WaitForInitialScanForTesting();

 private:
  // content::RenderProcessHostObserver:
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override;

  // Returns true if dynamic patching has not been disabled via enterprise
  // policy.
  bool IsDynamicPatchingEnabled() const
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  bool IsCircuitBreakerEnabled() const
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void LoadBlocklist() VALID_CONTEXT_REQUIRED(sequence_checker_);
  // Adds `version` to the blocklist and persists the updated list to Local
  // State.
  // TODO(crbug.com/558598893): Implement a smarter blocklist management
  // mechanism (e.g., cooldown/backoff retry, integration with component updater
  // to purge or re-download failing versions, or bounded retention).
  void BlockVersion(const base::Version& version,
                    base::TerminationStatus trigger_status)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void UpdateCircuitBreakerOnProcessExit(const base::Version& version,
                                         base::TerminationStatus status,
                                         int exit_code,
                                         base::TimeDelta elapsed_time)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void OnVersionSetChanged(VersionSet versions);

  const raw_ref<PrefService> local_state_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The directory holding the available versions; empty if it was not set.
  const base::FilePath modules_dir_;

  VersionSet available_versions_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_set<base::Version> blocked_versions_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::flat_map<base::Version, int> consecutive_crashes_
      GUARDED_BY_CONTEXT(sequence_checker_);

  base::ScopedMultiSourceObservation<content::RenderProcessHost,
                                     content::RenderProcessHostObserver>
      rph_observations_ GUARDED_BY_CONTEXT(sequence_checker_){this};

  base::SequenceBound<ChildModuleWatcher> watcher_
      GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ChildModuleManager> weak_factory_{this};
};

}  // namespace child_module

#endif  // CHROME_BROWSER_CHILD_MODULE_CHILD_MODULE_MANAGER_H_
