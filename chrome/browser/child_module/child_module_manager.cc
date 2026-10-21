// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/child_module/child_module_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/child_module/child_module_watcher.h"
#include "chrome/common/child_module/child_module_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_process_host.h"

namespace child_module {

namespace {

// Returns the directory in which child module versions are staged, or an empty
// path if it was not set.
base::FilePath GetChildModuleRoot() {
  base::FilePath modules_dir;
  base::PathService::Get(chrome::DIR_CHILD_MODULE_ROOT, &modules_dir);
  return modules_dir;
}

const void* const kChildModuleProcessUserDataKey =
    &kChildModuleProcessUserDataKey;

class ChildModuleProcessUserData : public base::SupportsUserData::Data {
 public:
  explicit ChildModuleProcessUserData(ChildModuleManager::Module module)
      : module_(std::move(module)) {}
  ChildModuleProcessUserData(const ChildModuleProcessUserData&) = delete;
  ChildModuleProcessUserData& operator=(const ChildModuleProcessUserData&) =
      delete;
  ~ChildModuleProcessUserData() override = default;

  const ChildModuleManager::Module& module() const { return module_; }

 private:
  const ChildModuleManager::Module module_;
};

}  // namespace

// static
void ChildModuleManager::RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDynamicPatchingEnabled, true);
}

ChildModuleManager::ChildModuleManager(PrefService& local_state)
    : local_state_(local_state), modules_dir_(GetChildModuleRoot()) {
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
  if (!IsDynamicPatchingEnabled() || available_versions_.empty()) {
    host->RemoveUserData(kChildModuleProcessUserDataKey);
    return std::nullopt;
  }
  const base::Version& version = *available_versions_.begin();
  auto user_data = std::make_unique<ChildModuleProcessUserData>(Module{
      .version = version,
      .renderer_binary_path = GetRendererBinaryPath(modules_dir_, version)});
  const Module& stored_module = user_data->module();
  host->SetUserData(kChildModuleProcessUserDataKey, std::move(user_data));
  return stored_module;
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

void ChildModuleManager::WaitForInitialScanForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  watcher_.FlushPostedTasksForTesting();  // IN-TEST
}

bool ChildModuleManager::IsDynamicPatchingEnabled() const {
  return local_state_->GetBoolean(prefs::kDynamicPatchingEnabled);
}

void ChildModuleManager::OnVersionSetChanged(VersionSet versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  available_versions_ = std::move(versions);
}

}  // namespace child_module
