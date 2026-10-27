// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/scoped_multi_source_observation.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/child_module/child_module_watcher.h"
#include "chrome/browser/child_module/features.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/result_codes.h"

namespace child_module {

namespace {

constexpr std::string_view kBlocklistBaseVersionKey = "base_version";
constexpr std::string_view kBlocklistVersionsKey = "versions";

// Returns the directory in which child module versions are staged, or an empty
// path if it was not set.
base::FilePath GetChildModuleRoot() {
  base::FilePath modules_dir;
  base::PathService::Get(chrome::DIR_CHILD_MODULE_ROOT, &modules_dir);
  return modules_dir;
}

const void* const kChildModuleProcessUserDataKey =
    &kChildModuleProcessUserDataKey;

// Tracks per-process module selection and lifecycle state for a
// RenderProcessHost across process launches and exits.
class ChildModuleProcessUserData : public base::SupportsUserData::Data {
 public:
  struct ProcessExitTrackingInfo {
    base::Version version;
    base::TimeTicks creation_time;
  };

  ChildModuleProcessUserData() = default;
  ChildModuleProcessUserData(const ChildModuleProcessUserData&) = delete;
  ChildModuleProcessUserData& operator=(const ChildModuleProcessUserData&) =
      delete;
  ~ChildModuleProcessUserData() override = default;

  base::optional_ref<const ChildModuleManager::Module> module() const {
    return module_;
  }

  // Records `selected_module` for a newly launching renderer process on the
  // host and returns a reference to the stored module (if any).
  base::optional_ref<const ChildModuleManager::Module> OnProcessLaunching(
      std::optional<ChildModuleManager::Module> selected_module) {
    switch (state_) {
      case State::kIdle:
        state_ = State::kRunning;
        break;
      case State::kRunning:
        if (module_.has_value()) {
          pending_exit_info_.emplace(std::move(module_->version),
                                     creation_time_);
        } else {
          pending_exit_info_.reset();
        }
        state_ = State::kReinitializedDuringExit;
        break;
      case State::kReinitializedDuringExit:
        // RenderProcessHostImpl::Init() returns early once initialized and not
        // dead, and ProcessDied() is non-reentrant, so a third launch cannot
        // occur before OnProcessExited() runs for the first process.
        NOTREACHED();
    }
    module_ = std::move(selected_module);
    creation_time_ = base::TimeTicks::Now();
    return module_;
  }

  // Advances the state machine for a renderer process exit and returns the
  // exited process's tracking info if it was running with a patched module.
  std::optional<ProcessExitTrackingInfo> OnProcessExited() {
    switch (state_) {
      case State::kIdle:
        NOTREACHED();
      case State::kRunning: {
        // The currently tracked process exited without a prior synchronous
        // relaunch.
        std::optional<ProcessExitTrackingInfo> exit_info;
        if (module_.has_value()) {
          exit_info.emplace(std::move(module_->version), creation_time_);
        }
        module_.reset();
        state_ = State::kIdle;
        return exit_info;
      }
      case State::kReinitializedDuringExit:
        // This exit pertains to the previous process; keep `module_` for the
        // already relaunched process.
        state_ = State::kRunning;
        return std::exchange(pending_exit_info_, std::nullopt);
    }
  }

 private:
  // State transitions:
  //
  //                                         ^
  //                                         | OnProcessExited -> NOTREACHED()
  //                                         |
  //                  +--------------------------+
  //                  |          kIdle           |
  //                  +--------------------------+
  //                    |                      ^
  //                    | OnProcessLaunching   | OnProcessExited
  //                    | (set module_)        | (take & return module_)
  //                    v                      |
  //                  +--------------------------+
  //                  |         kRunning         |
  //                  +--------------------------+
  //                    |                      ^
  //                    | OnProcessLaunching   | OnProcessExited
  //                    | (stash old module_;  | (keep module_; take &
  //                    |  set new module_)    |  return pending_exit_info_)
  //                    v                      |
  //                  +--------------------------+
  //                  | kReinitializedDuringExit |
  //                  +--------------------------+
  //                    |
  //                    | OnProcessLaunching -> NOTREACHED()
  //                    v
  enum class State {
    // No renderer process is currently running on this host.
    kIdle,
    // A renderer process is currently running on this host (`module_` holds its
    // patched module, or std::nullopt if it launched unpatched).
    kRunning,
    // A new renderer process was initialized via SelectModuleForProcess()
    // before RenderProcessExited() ran for the previous process.
    // `pending_exit_info_` holds the previous process's tracking info (if it
    // was patched), while `module_` and `creation_time_` hold the newly
    // launching process's state.
    kReinitializedDuringExit,
  };

  State state_ = State::kIdle;
  std::optional<ChildModuleManager::Module> module_;
  base::TimeTicks creation_time_;
  std::optional<ProcessExitTrackingInfo> pending_exit_info_;
};

}  // namespace

// static
void ChildModuleManager::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDynamicPatchingEnabled, true);
  registry->RegisterDictionaryPref(prefs::kDynamicPatchingBlocklist);
}

ChildModuleManager::ChildModuleManager(PrefService& local_state)
    : local_state_(local_state), modules_dir_(GetChildModuleRoot()) {
  LoadBlocklist();

  auto background_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  // The watcher does nothing if `modules_dir_` is empty.
  watcher_.emplace(std::move(background_task_runner), modules_dir_,
                   base::BindPostTaskToCurrentDefault(base::BindRepeating(
                       &ChildModuleManager::OnVersionSetChanged,
                       weak_factory_.GetWeakPtr())));
}

ChildModuleManager::~ChildModuleManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

base::optional_ref<const ChildModuleManager::Module>
ChildModuleManager::SelectModuleForProcess(
    content::ChildProcessId render_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    return std::nullopt;
  }

  std::optional<Module> selected_module;
  if (IsDynamicPatchingEnabled()) {
    const bool circuit_breaker_active = IsCircuitBreakerEnabled();
    for (const base::Version& version : available_versions_) {
      if (circuit_breaker_active && blocked_versions_.contains(version)) {
        continue;
      }
      selected_module.emplace(version,
                              GetRendererBinaryPath(modules_dir_, version));
      break;
    }
  }

  auto* user_data = static_cast<ChildModuleProcessUserData*>(
      host->GetUserData(kChildModuleProcessUserDataKey));
  if (!user_data) {
    auto new_user_data = std::make_unique<ChildModuleProcessUserData>();
    user_data = new_user_data.get();
    host->SetUserData(kChildModuleProcessUserDataKey, std::move(new_user_data));
  }

  // Dynamic child module execution is not supported and not planned on
  // ChromeOS, as dynamic patching itself is not needed on ChromeOS.
  // TODO(crbug.com/558598893): Exclude //chrome/browser/child_module from
  // ChromeOS builds entirely.
  //
  // TODO(crbug.com/558598893): Support macOS. Requires helper app bundle
  // resolution (Chromium Helper (Renderer).app) and Hardened Runtime / code
  // signing support.
#if !BUILDFLAG(IS_CHROMEOS)
  // Observe `host` unconditionally on first selection and keep the observation
  // until RenderProcessHostDestroyed() so that ChildModuleManager is never
  // added or removed mid-ProcessDied() iteration.
  if (!rph_observations_.IsObservingSource(host)) {
    rph_observations_.AddObservation(host);
  }
#endif  // !BUILDFLAG(IS_CHROMEOS)

  return user_data->OnProcessLaunching(std::move(selected_module));
}

base::optional_ref<const ChildModuleManager::Module>
ChildModuleManager::GetModuleForProcess(
    content::ChildProcessId render_process_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const content::RenderProcessHost* host =
      content::RenderProcessHost::FromID(render_process_id);
  if (!host) {
    return std::nullopt;
  }
  const auto* user_data = static_cast<const ChildModuleProcessUserData*>(
      host->GetUserData(kChildModuleProcessUserDataKey));
  if (!user_data) {
    return std::nullopt;
  }
  return user_data->module();
}

const VersionSet& ChildModuleManager::GetAvailableVersionsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return available_versions_;
}

const base::flat_set<base::Version>&
ChildModuleManager::GetBlockedVersionsForTesting() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return blocked_versions_;
}

void ChildModuleManager::WaitForInitialScanForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watcher_.FlushPostedTasksForTesting();  // IN-TEST
}

void ChildModuleManager::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto* user_data = static_cast<ChildModuleProcessUserData*>(
      host->GetUserData(kChildModuleProcessUserDataKey));
  if (!user_data) {
    return;
  }

  if (std::optional<ChildModuleProcessUserData::ProcessExitTrackingInfo>
          exit_info = user_data->OnProcessExited()) {
    UpdateCircuitBreakerOnProcessExit(
        exit_info->version, info.status, info.exit_code,
        base::TimeTicks::Now() - exit_info->creation_time);
  }
}

void ChildModuleManager::RenderProcessHostDestroyed(
    content::RenderProcessHost* host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rph_observations_.RemoveObservation(host);
}

bool ChildModuleManager::IsDynamicPatchingEnabled() const {
  return local_state_->GetBoolean(prefs::kDynamicPatchingEnabled);
}

bool ChildModuleManager::IsCircuitBreakerEnabled() const {
  return features::kCircuitBreakerMaxConsecutiveCrashes.Get() > 0;
}

void ChildModuleManager::LoadBlocklist() {
  blocked_versions_.clear();

  const base::DictValue& blocklist_dict =
      local_state_->GetDict(prefs::kDynamicPatchingBlocklist);
  if (blocklist_dict.empty()) {
    return;
  }

  const std::string* stored_base_version =
      blocklist_dict.FindString(kBlocklistBaseVersionKey);
  if (!stored_base_version || *stored_base_version != chrome::kChromeVersion) {
    local_state_->ClearPref(prefs::kDynamicPatchingBlocklist);
    return;
  }

  const base::ListValue* versions_list =
      blocklist_dict.FindList(kBlocklistVersionsKey);
  if (!versions_list) {
    local_state_->ClearPref(prefs::kDynamicPatchingBlocklist);
    return;
  }

  for (const auto& item : *versions_list) {
    if (const std::string* version_str = item.GetIfString()) {
      base::Version version(*version_str);
      if (version.IsValid()) {
        blocked_versions_.insert(std::move(version));
      }
    }
  }
}

void ChildModuleManager::BlockVersion(const base::Version& version,
                                      base::TerminationStatus trigger_status) {
  if (!IsCircuitBreakerEnabled() ||
      !blocked_versions_.emplace(version).second) {
    return;
  }

  // Record the 4th component (the patch version number, e.g. 51 in
  // 147.0.7727.51) of the blocked module's version.
  base::UmaHistogramSparse(
      "ChildProcess.ModuleCircuitBreaker.BlockedModulePatchVersion",
      base::saturated_cast<int>(version.components()[3]));

  base::UmaHistogramEnumeration(
      "ChildProcess.ModuleCircuitBreaker.TriggerTerminationStatus",
      trigger_status, base::TERMINATION_STATUS_MAX_ENUM);

  base::ListValue versions_list =
      base::ListValue::with_capacity(blocked_versions_.size());
  for (const auto& v : blocked_versions_) {
    versions_list.Append(v.GetString());
  }

  base::DictValue blocklist_dict;
  blocklist_dict.Set(kBlocklistBaseVersionKey, chrome::kChromeVersion);
  blocklist_dict.Set(kBlocklistVersionsKey, std::move(versions_list));

  local_state_->SetDict(prefs::kDynamicPatchingBlocklist,
                        std::move(blocklist_dict));
}

void ChildModuleManager::UpdateCircuitBreakerOnProcessExit(
    const base::Version& version,
    base::TerminationStatus status,
    int exit_code,
    base::TimeDelta elapsed_time) {
  if (!IsCircuitBreakerEnabled() || blocked_versions_.contains(version)) {
    return;
  }

  // In this MVP, reliable fallback for unstable or incompatible renderers takes
  // precedence over theoretical downgrade attack mitigation. Terminations due
  // to bad messages (such as incompatible Mojo calls) are counted as failures
  // within the fast-crash threshold to ensure timely fallback.
  // TODO(crbug.com/558598893): Explore smarter mechanisms to mitigate potential
  // renderer-driven downgrade attacks in the future.
  const bool is_crash =
      (status == base::TERMINATION_STATUS_PROCESS_CRASHED ||
       status == base::TERMINATION_STATUS_ABNORMAL_TERMINATION ||
       status == base::TERMINATION_STATUS_LAUNCH_FAILED ||
       exit_code == content::RESULT_CODE_KILLED_BAD_MESSAGE
#if BUILDFLAG(IS_WIN)
       || status == base::TERMINATION_STATUS_INTEGRITY_FAILURE
#endif
      );

  if (is_crash &&
      elapsed_time <= features::kCircuitBreakerFastCrashThreshold.Get()) {
    int crashes = ++consecutive_crashes_[version];
    if (crashes >= features::kCircuitBreakerMaxConsecutiveCrashes.Get()) {
      consecutive_crashes_.erase(version);
      BlockVersion(version, status);
    }
  } else if (elapsed_time > features::kCircuitBreakerFastCrashThreshold.Get() ||
             status == base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    consecutive_crashes_.erase(version);
  }
}

void ChildModuleManager::OnVersionSetChanged(VersionSet versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Prune failure tracking for versions that are no longer staged on disk.
  base::EraseIf(consecutive_crashes_, [&versions](const auto& pair) {
    return !versions.contains(pair.first);
  });
  available_versions_ = std::move(versions);
}

}  // namespace child_module
